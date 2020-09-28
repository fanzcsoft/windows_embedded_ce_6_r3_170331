//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//
//    vm.c - VM implementations
//
#include <kernel.h>
#include <pager.h>

#ifdef SHx
#undef  PG_PERMISSION_MASK
#define PG_PERMISSION_MASK 0x00000FFF
#endif

const volatile DWORD gdwFailPowerPaging = 0;  // Can be overwritten as a FIXUPVAR in config.bib


#define VM_KPAGE_IDX        5

//
// This is to limit total number of process/threads in the system such that we don't use up
// kernel VM.
//
#define MAX_STACK_ADDRESS       (VM_CPU_SPECIFIC_BASE - 0x04000000)     // leave at least 64M of VM for kernel

#define VM_PD_DUP_ADDR          (VM_SHARED_HEAP_BASE - 0x400000)        // last 4M of user VM
#define VM_PD_DUP_SIZE          0x200000                                // 2M total size

#define VM_USER_LIMIT           VM_PD_DUP_ADDR

PSTATICMAPPINGENTRY g_StaticMappingList = NULL;

void MDInitStack (LPBYTE lpStack, DWORD cbSize);

static DWORD VMRelease (PPROCESS pprc, DWORD dwAddr, DWORD cPages);

static DWORD PagerNone (PPROCESS pprc, DWORD dwAddr, BOOL fWrite)
{
    return PAGEIN_FAILURE;
}

extern CRITICAL_SECTION PhysCS;

#define LockPhysMem()       EnterCriticalSection(&PhysCS)
#define UnlockPhysMem()     LeaveCriticalSection(&PhysCS)

//
// VM locking functions
//
__inline void UnlockVM (PPROCESS pprc)
{
    LeaveCriticalSection (&pprc->csVM);
}

__inline PPAGEDIRECTORY LockVM (PPROCESS pprc)
{
    EnterCriticalSection (&pprc->csVM);
    if (pprc->ppdir) {
        return pprc->ppdir;
    }
    LeaveCriticalSection (&pprc->csVM);
    return NULL;
}
static BOOL Lock2VM (PPROCESS pprc1, PPROCESS pprc2)
{
    if (pprc1->dwId < pprc2->dwId) {
        EnterCriticalSection (&pprc2->csVM);
        EnterCriticalSection (&pprc1->csVM);
    } else {
        EnterCriticalSection (&pprc1->csVM);
        EnterCriticalSection (&pprc2->csVM);
    }
    if (pprc1->ppdir && pprc2->ppdir) {
        return TRUE;
    }
    LeaveCriticalSection (&pprc1->csVM);
    LeaveCriticalSection (&pprc2->csVM);
    return FALSE;
}

typedef DWORD (* PFNPager) (PPROCESS pprc, DWORD dwAddr, BOOL fWrite);

static DWORD PageAutoCommit (PPROCESS pprc, DWORD dwAddr, BOOL fWrite);

//
// the following arrays Must match the VM_PAGER_XXX declaration in vm.h
//

// the pager function
static PFNPager g_vmPagers[]  = { PagerNone,   PageFromLoader, PageFromMapper, PageAutoCommit };
// memory type based on pager
static DWORD    g_vmMemType[] = { MEM_PRIVATE, MEM_IMAGE,      MEM_MAPPED,     MEM_PRIVATE    };

static DWORD g_vaDllFree        = VM_DLL_BASE;              // lowest free VA for DLL allocaiton
static DWORD g_vaSharedHeapFree = VM_SHARED_HEAP_BASE;      // lowest free VA for shared heap
static DWORD g_vaRAMMapFree     = VM_RAM_MAP_BASE;          // lowest free VA for RAM-backed memory-mappings
static DWORD g_vaKDllFree       = VM_KDLL_BASE;             // lowest free VA for Kernel XIP DLLs

// total number of stack cached
static LONG g_nStackCached;
static PSTKLIST g_pStkList;

const DWORD idxPDSearchLimit = VA2PDIDX (VM_CPU_SPECIFIC_BASE);


//-----------------------------------------------------------------------------------------------------------------
//
// type of page table enumeration function
//
typedef BOOL (*PFN_PTEnumFunc) (LPDWORD pdwEntry, LPDWORD pEnumData);


//-----------------------------------------------------------------------------------------------------------------
//
//  IsValidAllocType: check if allocation type is valid
//
__inline BOOL IsValidAllocType (DWORD dwAddr, DWORD fAllocType)
{
    fAllocType &= ~MEM_TOP_DOWN;        // MEM_TOP_DOWN is ignored
    return fAllocType && !(fAllocType & ~VM_VALID_ALLOC_TYPE);
}

//-----------------------------------------------------------------------------------------------------------------
//
//  IsValidProtect: check if protection is valid
//
//  NOTE: the bottom 8 bit of the protection is mutually exclusive. i.e. there
//        must be exactly one bit set in the bottom 8 bits
//
__inline BOOL IsValidProtect (DWORD fProtect)
{
    DWORD dwExclusive = fProtect & 0xff;
    return dwExclusive
        && !(dwExclusive & (dwExclusive - 1))
        // PAGE_NOACCESS is not valid with other flag set
        && (!(fProtect & PAGE_NOACCESS) || (PAGE_NOACCESS == fProtect))
#ifdef x86
        && !(fProtect & ~(VM_VALID_PROT|PAGE_x86_WRITETHRU));
#else
        && !(fProtect & ~VM_VALID_PROT);
#endif
}


//-----------------------------------------------------------------------------------------------------------------
//
//  IsValidFreeType: check if free type is valid
//
__inline BOOL IsValidFreeType (DWORD dwAddr, DWORD cbSize, DWORD dwFreeType)
{
    return (MEM_DECOMMIT == dwFreeType)
        || ((MEM_RELEASE == dwFreeType) && !cbSize && !(dwAddr & VM_BLOCK_OFST_MASK));
}

//-----------------------------------------------------------------------------------------------------------------
//
//  IsValidLockOpt: check if lock option is valid
//
__inline BOOL IsValidLockOpt (DWORD fOption)
{
    return !(fOption & ~(VM_LF_QUERY_ONLY | VM_LF_READ | VM_LF_WRITE));
}


//-----------------------------------------------------------------------------------------------------------------
//
// MemTypeFromAddr - get memory type based on address
//
static DWORD MemTypeFromAddr (PPROCESS pprc, DWORD dwAddr)
{
    






    return MEM_PRIVATE;
}

//-----------------------------------------------------------------------------------------------------------------
//
// MemTypeFromReservation - get memory type based on reservation
//
__inline DWORD MemTypeFromReservation (DWORD dwEntry)
{
    return g_vmMemType[GetPagerType (dwEntry)];
}


//-----------------------------------------------------------------------------------------------------------------
//
//  EntryFromReserveType: return entry, based on resevation type
//
static BOOL EntryFromReserveType (DWORD fReserveType)
{
    DWORD dwPagerType;
    if (fReserveType & MEM_AUTO_COMMIT) {
        dwPagerType = VM_PAGER_AUTO;
    } else if (fReserveType & MEM_IMAGE) {
        dwPagerType = VM_PAGER_LOADER;
    } else if (fReserveType & MEM_MAPPED) {
        dwPagerType = VM_PAGER_MAPPER;
    } else {
        dwPagerType = VM_PAGER_NONE;
    }
    return MakeReservedEntry (dwPagerType);
}

static PVALIST FindVAItem (PVALIST *ppListHead, DWORD dwAddr, BOOL fRemove)
{
    PVALIST *ppTrav, pTrav;

    for (ppTrav = ppListHead; pTrav = *ppTrav; ppTrav = &pTrav->pNext) {

        if (dwAddr - (DWORD) pTrav->pVaBase < pTrav->cbVaSize) {
            if (fRemove) {
                // base address must match if remove is specified.
                if (dwAddr == (DWORD) pTrav->pVaBase) {
                    *ppTrav = pTrav->pNext;
                } else {
                    pTrav = NULL;
                }
            }
            break;
        }
    }
    return pTrav;
}

BOOL VMFindAlloc (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, BOOL fRelesae)
{
    BOOL fRet = FALSE;
    DEBUGCHK (!fRelesae || !cbSize);
    if (((pprc == g_pprcNK) || ((DWORD) pAddr < VM_RAM_MAP_BASE))       // valid address?
        && ((int) cbSize >= 0)                                          // valid size?
        && LockVM (pprc)) {

        PVALIST pVaItem = FindVAItem (&pprc->pVaList, (DWORD) pAddr, fRelesae);

        fRet = pVaItem
            && ((DWORD) pAddr - (DWORD) pVaItem->pVaBase + cbSize <= pVaItem->cbVaSize);

#ifdef ARM
        if (fRet && fRelesae) {
            PVALIST pUncachedItem = FindVAItem (&pprc->pUncachedList, (DWORD) pAddr, fRelesae);
            if (pUncachedItem) {
                if (pUncachedItem->pPTE) {
                    NKfree (pUncachedItem->pPTE);
                }
                FreeMem (pUncachedItem, HEAP_VALIST);
            }
        }
#endif
        UnlockVM (pprc);

        if (fRet && fRelesae) {
            DEBUGCHK (pVaItem && !cbSize);
            VERIFY (VMFreeAndRelease (pprc, pAddr, pVaItem->cbVaSize));
#ifdef ARM
            VMUnalias (pVaItem->pAlias);
#endif
            FreeMem (pVaItem, HEAP_VALIST);
        }
    }
    return fRet;
}


void VMFreeAllocList (PPROCESS pprc)
{
    if (LockVM (pprc)) {
        PVALIST pVaList;
        PVALIST pVaItem;
#ifdef ARM
        PVALIST pUncachedList;
#endif

        pprc->bState = PROCESS_STATE_VM_CLEARED;
        pVaList = pprc->pVaList;
        pprc->pVaList = NULL;
#ifdef ARM
        pUncachedList = pprc->pUncachedList;
        pprc->pUncachedList = NULL;
#endif
        UnlockVM (pprc);

        while (pVaItem = pVaList) {
            pVaList = pVaItem->pNext;
#ifdef ARM
            // for ARM, we need to call VMFreeAndRelease to decrement physical ref-count before
            // calling VMUnalias, or the ref-count will be > 1 and we won't be able to turn cache
            // back on.
            VERIFY (VMFreeAndRelease (pprc, pVaItem->pVaBase, pVaItem->cbVaSize));
            VMUnalias (pVaItem->pAlias);
#endif
            FreeMem (pVaItem, HEAP_VALIST);
        }

#ifdef ARM
        while (pVaItem = pUncachedList) {
            pUncachedList = pVaItem->pNext;
            if (pVaItem->pPTE) {
                NKfree (pVaItem->pPTE);
            }
            FreeMem (pVaItem, HEAP_VALIST);
        }
#endif
    }
}


BOOL VMAddAllocToList (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, PALIAS pAlias)
{
    PVALIST pVaItem = AllocMem (HEAP_VALIST);
    BOOL fRet = FALSE;

    if (pVaItem) {

#ifdef VM_DEBUG
        if (g_pfnKrnRtlDispExcp) {
            THRDGetCallStack (pCurThread, MAX_CALLSTACK, &pVaItem->callstack[0], 0, 2);
        }
#endif
        pVaItem->pVaBase  = pAddr;
        pVaItem->cbVaSize = cbSize;
        pVaItem->pAlias   = pAlias;
        if (LockVM (pprc)) {
            DEBUGCHK (IsVMAllocationAllowed (pprc));
            if (IsVMAllocationAllowed (pprc)) {
                pVaItem->pNext    = pprc->pVaList;
                pprc->pVaList     = pVaItem;
                fRet = TRUE;
            }
            UnlockVM (pprc);
        }

        if (!fRet) {
            FreeMem (pVaItem, HEAP_VALIST);
        }
    }

    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  FindReserveBase: return reservation based of a particular 2nd level page table entry
//
static DWORD FindReserveBase (PPROCESS pprc, DWORD dwAddr)
{
    DWORD dwBase = dwAddr;

    if (LockVM (pprc)) {

        PVALIST pvalc = FindVAItem (&pprc->pVaList, dwAddr, FALSE);

        if (pvalc) {
            dwBase = (DWORD) pvalc->pVaBase;
        }

        UnlockVM (pprc);
    }

    return dwBase;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  Enumerate2ndPT: Enumerate 2nd-level page table, pass pointer to 2nd level page table entry to
//                  the 'apply' function. Enumeration stop right away and returns false if the apply function
//                  returns false. Or returns ture if it finishes enumerate cPages
//
static BOOL Enumerate2ndPT (PPAGEDIRECTORY ppdir, DWORD dwAddr, DWORD cPages, BOOL fWrite, PFN_PTEnumFunc pfnApply, LPVOID pEnumData)
{
    DWORD       idxdir = VA2PDIDX (dwAddr);   // 1st-level PT index
    PPAGETABLE  pptbl  = Entry2PTBL (ppdir->pte[idxdir]);
    DWORD       cpLeft = cPages;

    DEBUGCHK (dwAddr);
    DEBUGCHK ((int) cPages > 0);
    DEBUGCHK (pfnApply);
    DEBUGCHK (ppdir);

    if (pptbl) {

#ifdef ARM
        DWORD idxPteChanged = VM_NUM_PT_ENTRIES;
#endif
        DWORD idx2nd = VA2PT2ND (dwAddr);   // 2nd-level PT index
        DWORD dwEntry;

        for ( ; ; ) {

            DEBUGMSG (ZONE_VIRTMEM, (L"Enumerate2ndPT: idxdir = %8.8lx, idx2nd = %8.8lx, ppdir->pte[idxdir] = %8.8lx\r\n",
                idxdir, idx2nd, ppdir->pte[idxdir]));
            PREFAST_ASSUME (idx2nd < VM_NUM_PT_ENTRIES);
            dwEntry = pptbl->pte[idx2nd];

            if (fWrite                                                  // asking for write permission
                && IsPageCommitted (dwEntry)                            // already commited
                && g_pOemGlobal->pfnIsRom (PA256FromPfn (PFNfromEntry (dwEntry)))) {     // not writeable
                // can't do R/W on ROM
                break;
            }

            // apply the function to a 2nd level page table entry
            if (!pfnApply (&pptbl->pte[idx2nd], pEnumData)) {
                break;
            }
            DEBUGMSG (ZONE_VIRTMEM, (L"Enumerate2ndPT: &pptbl->pte[idx2nd] = %8.8lx, pptbl->pte[idx2nd] = %8.8lx\r\n",
                &pptbl->pte[idx2nd], pptbl->pte[idx2nd]));

#ifdef ARM
            if ((dwEntry != pptbl->pte[idx2nd])                         // PTE updated
                && (VM_NUM_PT_ENTRIES == idxPteChanged)                 // 1st change in this table?
                && ((dwEntry | pptbl->pte[idx2nd]) & PG_VALID_MASK)) {  // at least one of old/new value is valid
                // page table entry changed. record the index
                idxPteChanged = idx2nd;
            }
#endif
            // move to next index
            idx2nd ++;

            // done?
            if (! -- cpLeft) {
                break;
            }

            // if we're at the end of the PT, move to the next
            if (VM_NUM_PT_ENTRIES == idx2nd) {

#ifdef ARM
                if (idxPteChanged < idx2nd) {
                    ARMPteUpdateBarrier (&pptbl->pte[idxPteChanged], idx2nd - idxPteChanged);
                    idxPteChanged = VM_NUM_PT_ENTRIES;
                }
#endif

                idxdir = NextPDEntry (idxdir);
                if (VM_NUM_PD_ENTRIES == idxdir) {
                    // end of 1st level page table, shouldn't get here for
                    // we should've encounter "end of reservation".
                    DEBUGCHK (0);
                    break;
                }
                pptbl = Entry2PTBL (ppdir->pte[idxdir]);
                if (!pptbl) {
                    // completely un-touched directory (no page-table), fail the enumeration.
                    break;
                }
                // continue from the beginning of the new PT
                idx2nd = 0;
            }

        }
#ifdef ARM
        if (idxPteChanged < idx2nd) {
            DEBUGCHK (pptbl);
            ARMPteUpdateBarrier (&pptbl->pte[idxPteChanged], idx2nd - idxPteChanged);
        }
#endif
    }

    return !cpLeft;
}



//-----------------------------------------------------------------------------------------------------------------
//
//  CountFreePagesInSection: given a 1st-level PT entry, count the number of free pages in this section,
//                           starting from page of index 'idx2nd'.
//  returns: # of free pages
//
static DWORD CountFreePagesInSection (DWORD dwPDEntry, DWORD idx2nd)
{
    DWORD cPages = 0;
    PPAGETABLE pptbl = Entry2PTBL (dwPDEntry);

    DEBUGCHK (!(idx2nd & 0xf));     // idx2nd must be multiple of VM_PAGES_PER_BLOCK

    if (pptbl) {

        // 2nd level page table exist, scan the free pages
        cPages = 0;
        while ((idx2nd < VM_NUM_PT_ENTRIES) && (VM_FREE_PAGE == pptbl->pte[idx2nd])) {
            idx2nd += VM_PAGES_PER_BLOCK;
            cPages += VM_PAGES_PER_BLOCK;
        }
    } else if (!IsSectionMapped (dwPDEntry)) {

        // 1st level page table entry is 0, the full 4M section is free
        cPages = VM_NUM_PT_ENTRIES - idx2nd;

    }

    return cPages;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  CountFreePages: count the number of free pages starting from dwAddr, stop if exceeds cPages.
//  returns: # of free pages in block granularity
//
static DWORD CountFreePages (PPAGEDIRECTORY ppdir, DWORD dwAddr, DWORD cPages)
{
    DWORD idxdir = VA2PDIDX (dwAddr);  // 1st-level PT index
    DWORD idx2nd = VA2PT2ND (dwAddr);  // 2nd-level PT index

    DWORD nTotalFreePages, nCurFreePages;

    DEBUGCHK (ppdir);
    DEBUGCHK (!(dwAddr & (VM_BLOCK_OFST_MASK)));   // must be 64K aligned
    DEBUGCHK (dwAddr);
    DEBUGCHK ((int) cPages > 0);

    for (nTotalFreePages = 0; idxdir < idxPDSearchLimit; idxdir = NextPDEntry (idxdir), idx2nd = 0) {
        nCurFreePages = CountFreePagesInSection (ppdir->pte[idxdir], idx2nd);
        nTotalFreePages += nCurFreePages;

        if ((nTotalFreePages >= cPages)                             // got enough page?
            || (VM_NUM_PT_ENTRIES - idx2nd != nCurFreePages)) {     // can we span to the next 'section'?
            break;
        }
    }

    return nTotalFreePages;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  CountUsedPages: count the number of used pages starting from dwAddr.
//  returns: # of used pages in block granularity (e.g. return 16 if there are 10 used pages)
//
static DWORD CountUsedPages (PPAGEDIRECTORY ppdir, DWORD dwAddr)
{
    DWORD idxdir = VA2PDIDX (dwAddr);  // 1st-level PT index
    DWORD idx2nd = VA2PT2ND (dwAddr);  // 2nd-level PT index
    PPAGETABLE pptbl;
    DWORD cPages;

    DEBUGCHK (!(dwAddr & (VM_BLOCK_OFST_MASK)));   // must be 64K aligned
    DEBUGCHK (dwAddr);
    DEBUGCHK (ppdir);

    // iterate through the 1st level page table entries
    for (cPages = 0; (idxdir < idxPDSearchLimit) && ppdir->pte[idxdir]; idxdir = NextPDEntry (idxdir), idx2nd = 0) {

        if (pptbl = Entry2PTBL (ppdir->pte[idxdir])) {

            // iterate through the 2nd level page tables, 16 page at a time
            while ((idx2nd < VM_NUM_PT_ENTRIES) && (VM_FREE_PAGE != pptbl->pte[idx2nd])) {
                idx2nd += VM_PAGES_PER_BLOCK;
                cPages += VM_PAGES_PER_BLOCK;
            }
            // check if this is the end of reservation
            if ((idx2nd < VM_NUM_PT_ENTRIES) || !pptbl->pte[VM_NUM_PT_ENTRIES-1]) {
                break;
            }
        } else {
            DEBUGCHK (IsSectionMapped (ppdir->pte[idxdir]));
            cPages += VM_NUM_PT_ENTRIES - idx2nd;
        }

    }

    return cPages;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  SearchFreePages: search from dwAddr till dwLimit for cPages of consecutive free pages
//
static DWORD SearchFreePages (PPAGEDIRECTORY ppdir, LPDWORD pdwBase, DWORD dwLimit, DWORD cPages)
{
    DWORD cpFound = 0;
    DWORD dwAddr  = *pdwBase;

    DEBUGCHK (dwAddr);
    DEBUGCHK ((int) cPages > 0);
    DEBUGMSG (ZONE_VIRTMEM, (L"SearchFreePages: Search free pages from 0x%8.8lx for 0x%x pages, limit = 0x%8.8lx\r\n", dwAddr, cPages, dwLimit));

    if (dwAddr < dwLimit) {
        dwAddr += CountUsedPages (ppdir, dwAddr) << VM_PAGE_SHIFT;
    }

    if (dwAddr < dwLimit) {

        // base can be starting from an allocated area, change it to the 1st free area
        *pdwBase = dwAddr;

        // search from dwAddr until dwLimit, find a chunk of free VM
        while ((dwAddr < dwLimit) && ((cpFound = CountFreePages (ppdir, dwAddr, cPages)) < cPages)) {

            DEBUGCHK (!(dwAddr & (VM_BLOCK_OFST_MASK)));   // must be 64K aligned

            // skip all used pages and the found free pages that can't satisfy the request
            dwAddr += cpFound << VM_PAGE_SHIFT;
            dwAddr += CountUsedPages (ppdir, dwAddr) << VM_PAGE_SHIFT;

        }
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"SearchFreePages: returns 0x%8.8lx\r\n", (cpFound >= cPages)? dwAddr : 0));
    return ((cpFound >= cPages) && (dwAddr < dwLimit))? dwAddr : 0;

}

//-----------------------------------------------------------------------------------------------------------------
//
//  CountPagesNeeded: 2nd level page table enumeration funciton, count uncommitted pages
//
static BOOL CountPagesNeeded (LPDWORD pdwEntry, LPVOID pEnumData)
{
    LPDWORD pcPagesNeeded = (LPDWORD) pEnumData;

    if (VM_FREE_PAGE == *pdwEntry) {
        return FALSE;
    }

    // update page needed if not committed
    if (!IsPageCommitted (*pdwEntry)) {
        // not commited
        *pcPagesNeeded += 1;
    }
    return TRUE;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  VMScan: scan VM from dwAddr for cPages, count # of uncommitted pages
//  returns: dwAddr if cPages were found in the same VM reservation
//           0 if there are < cPages in the same VM reservation
//
static DWORD VMScan (PPROCESS pprc, DWORD dwAddr, DWORD cPages, LPDWORD pcPagesNeeded, BOOL fWrite)
{
    DEBUGCHK (dwAddr);                          // dwAddr cannot be NULL
    DEBUGCHK ((int) cPages > 0);                // at least 1 page
    DEBUGCHK (!(dwAddr & VM_PAGE_OFST_MASK));   // dwAddr must be page aligned

    *pcPagesNeeded = 0;

    return Enumerate2ndPT (pprc->ppdir, dwAddr, cPages, fWrite, CountPagesNeeded, pcPagesNeeded)
        ? dwAddr
        : 0;

}

#define VM_MAP_LIMIT        0xFFFE0000

//-----------------------------------------------------------------------------------------------------------------
//
//  DoVMReserve: reserve VM
//
static DWORD DoVMReserve (PPROCESS pprc, DWORD dwAddr, DWORD cPages, DWORD dwSearchBase, DWORD fAllocType)
{
    PPAGEDIRECTORY  ppdir = pprc->ppdir;      // 1st level page table
    LPDWORD  pdwBaseFree = &pprc->vaFree;
    DEBUGCHK ((int) cPages > 0);
    DEBUGCHK (ppdir);

    DEBUGCHK (!dwSearchBase || !dwAddr);
    DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - process-id: %8.8lx, dwAddr = %8.8lx, cPages = 0x%x, base = %8.8lx, fAllocType = %8.8lx\r\n",
        pprc->dwId, dwAddr, cPages, dwSearchBase, fAllocType));

    if (dwAddr) {
        dwAddr &= ~VM_BLOCK_OFST_MASK;

        if (dwAddr >= VM_SHARED_HEAP_BASE) {
            ppdir = g_ppdirNK;

        } else if ((ppdir == g_ppdirNK)
            && (dwAddr >= VM_DLL_BASE)
            && (dwAddr <  VM_RAM_MAP_BASE)) {
            // reserving user mode DLL address
            pdwBaseFree = &g_vaDllFree;
        }

        // specific address given, verify the range is free
        if (CountFreePages (ppdir, dwAddr, cPages) < cPages) {
            dwAddr = 0;
        }

    } else {
        // no range given, find whatever fit
        DWORD dwLimit;

        switch (dwSearchBase) {
        case VM_DLL_BASE:
            pdwBaseFree = &g_vaDllFree;
            dwLimit = VM_RAM_MAP_BASE;
            break;
        case VM_RAM_MAP_BASE:
            pdwBaseFree = &g_vaRAMMapFree;
            dwLimit = VM_USER_LIMIT;
            break;
        case VM_SHARED_HEAP_BASE:
            pdwBaseFree = &g_vaSharedHeapFree;
            dwLimit = VM_KMODE_BASE;
            ppdir = g_ppdirNK;
            break;
        case VM_KDLL_BASE:
            pdwBaseFree  = &g_vaKDllFree;
            dwLimit = VM_OBJSTORE_BASE;
            ppdir = g_ppdirNK;
            break;
        default:
            dwLimit = (IsInKVM (*pdwBaseFree) ? VM_CPU_SPECIFIC_BASE : VM_DLL_BASE);
            break;
        }

        DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - searching from va = %8.8lx\r\n", *pdwBaseFree));

        dwAddr = SearchFreePages (ppdir, pdwBaseFree, dwLimit, cPages);
#ifdef VM_DEBUG
        if (!dwAddr
            && !dwSearchBase
            && (ppdir != g_ppdirNK)) {
            // we "slotize" user VM to try to catch references to other process's memory.
            // As a result, we need to do a second pass searching for free pages
            dwLimit = *pdwBaseFree;
            *pdwBaseFree = VM_USER_BASE;
            dwAddr = SearchFreePages (ppdir, pdwBaseFree, dwLimit, cPages);
        }
#endif
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - use Addr %8.8lx\r\n", dwAddr));

    DEBUGMSG (!dwAddr, (L"!!!! Out of VM for process %8.8lx !!!!\r\n", pprc->dwId));
    DEBUGCHK (dwAddr);

    if (dwAddr) {
        // found enough free pages to reserve
        PPAGETABLE  pptbl;                        // 2nd level page table
        DWORD dwAddrEnd = dwAddr + (cPages << VM_PAGE_SHIFT) - 1;   // end address
        DWORD idxdirEnd = VA2PDIDX (dwAddrEnd);     // end idx for 1st level PT
        DWORD idx;

        // allocate 2nd level page tables if needed
        for (idx = VA2PDIDX (dwAddr); idx <= idxdirEnd; idx = NextPDEntry (idx)) {
            if (!ppdir->pte[idx]) {
                DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - allocating 2nd-level page table\r\n"));
                if (!(pptbl = AllocatePTBL ())) {
                    // out of memory
                    dwAddr = 0;
                    break;
                }
                DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - 2nd-level page table at %8.8lx\r\n", pptbl));
                SetupPDEntry (ppdir, idx, pptbl, (dwAddr >= VM_SHARED_HEAP_BASE));
            }
        }

        if (dwAddr) {

            int   idx2nd  = VA2PT2ND (dwAddrEnd);               // idx to 2nd level PT
            DWORD dwEntry = EntryFromReserveType (fAllocType);  // entry value for the reserved page

            pptbl = Entry2PTBL (ppdir->pte[idxdirEnd]);
            DEBUGCHK (pptbl);

            DEBUGMSG (ZONE_VIRTMEM, (L"last page table at %8.8lx\r\n", pptbl));

            pptbl->pte[idx2nd] = dwEntry;
//            DEBUGMSG (ZONE_VIRTMEM, (L"pptbl->pte[0x%x] (addr %8.8lx) set to %8.8lx\r\n", idx2nd, &pptbl->pte[idx2nd], dwEntry));

            // walking backward for to update the entries
            while (--cPages) {

                if (--idx2nd < 0) {
                    idxdirEnd = PrevPDEntry (idxdirEnd);
                    pptbl = Entry2PTBL (ppdir->pte[idxdirEnd]);
                    idx2nd  = VM_NUM_PT_ENTRIES - 1;
                    DEBUGMSG (ZONE_VIRTMEM, (L"move to previous page table at %8.8lx\r\n", pptbl));

                }
                pptbl->pte[idx2nd] = dwEntry;
//                DEBUGMSG (ZONE_VIRTMEM, (L"pptbl->pte[0x%x] (addr %8.8lx) set to %8.8lx\r\n", idx2nd, &pptbl->pte[idx2nd], dwEntry));
            }

            // update pprc->vaFree if necessary
            if (dwAddr == *pdwBaseFree) {
                // round up to the next block
                *pdwBaseFree = (dwAddrEnd + VM_BLOCK_SIZE- 1) & -VM_BLOCK_SIZE;
            }
        }
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - returns %8.8lx\r\n", dwAddr));
    return dwAddr;
}


typedef struct {
    DWORD dwPgProt;
    DWORD dwPageType;
    BOOL  fFlush;
} VMCommitStruct, *PVMCommitStruct;

//-----------------------------------------------------------------------------------------------------------------
//
//  CommitPages: 2nd level page table enumeration funciton, uncommitted all uncommitted pages
//  NOTE: the required number of pages had been held prior to the enumeration
//
static BOOL CommitPages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVMCommitStruct pvs = (PVMCommitStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    DWORD dwPFN;

    if (IsPageCommitted (dwEntry)) {
        dwPFN = PFNfromEntry (dwEntry);
        pvs->fFlush |= !IsSameEntryType (dwEntry, pvs->dwPgProt);
    } else {
        dwPFN = GetHeldPage (pvs->dwPageType);
    }

    *pdwEntry = MakeCommittedEntry (dwPFN, pvs->dwPgProt);

#ifdef ARM
    ARMPteUpdateBarrier (pdwEntry, 1);
#endif

    return TRUE;

}

//-----------------------------------------------------------------------------------------------------------------
//
// DoVMAlloc: worker function to allocate VM.
//
static LPVOID
DoVMAlloc(
    PPROCESS pprc,          // the process where VM is to be allocated
    DWORD dwAddr,           // starting address
    DWORD cPages,           // # of pages
    DWORD fAllocType,       // allocation type
    DWORD fProtect,         // protection
    DWORD dwPageType,       // when committing, what type of page to use
    LPDWORD pdwErr          // error code if failed
    )
{
    DWORD dwRet = 0;
    DWORD cPageNeeded = cPages;

    // if dwAddr == 0, must reserve first
    if (!dwAddr) {
        fAllocType |= MEM_RESERVE;
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"DoVMAlloc - proc-id: %8.8lx, dwAddr = %8.8lx, cPages = 0x%x, fAllocType = %8.8lx, dwPageType = %8.8lx\r\n",
        pprc->dwId, dwAddr, cPages, fAllocType, dwPageType));

    if ((MEM_COMMIT & fAllocType)                   // commiting page
        && g_nStackCached                           // got some cached stacks
        && WillTriggerPageOut ((long) cPages)) {    // the request will trigger a pageout.

        // free all cached stacks
        VMFreeExcessStacks (0);
    }

    if (LockVM (pprc)) {

        dwRet = (fAllocType & MEM_RESERVE)
            ? DoVMReserve (pprc, dwAddr, cPages, 0, fAllocType)         // reserving VM
            : VMScan (pprc, dwAddr, cPages, &cPageNeeded, fProtect & VM_READWRITE_PROT);    // not reserving, must be commiting. Check if
                                                                        // the range to be commited is from a single reservation.
                                                                        // count #of page needed to commit

        if (!dwRet) {
            *pdwErr = dwAddr? ERROR_INVALID_PARAMETER: ERROR_NOT_ENOUGH_MEMORY;

        } else if (fAllocType & MEM_COMMIT) {
            if (cPageNeeded && !HoldPages (cPageNeeded, FALSE)) {
                *pdwErr = ERROR_NOT_ENOUGH_MEMORY;
            } else {
                VMCommitStruct vs = { PageParamFormProtect (fProtect, dwRet), dwPageType, FALSE };
                // got enough pages, update entries
                Enumerate2ndPT (pprc->ppdir, dwRet, cPages, 0, CommitPages, &vs);

                if (PAGE_NOCACHE & fProtect) {
                    // Flush with uncached VA can cause Cortex-A8 hanged so just flush all.
                    //OEMCacheRangeFlush ((LPVOID) dwRet, cPages << VM_PAGE_SHIFT, CACHE_SYNC_DISCARD | CACHE_SYNC_L2_DISCARD);
                    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD | CACHE_SYNC_L2_DISCARD);
                }

                if (vs.fFlush) {
                    InvalidatePages (pprc, dwRet, cPages);
                }
            }
        }

        UnlockVM (pprc);

        if (*pdwErr && dwRet) {
            // fail to commit enough pages
            if (fAllocType & MEM_RESERVE) {
                VERIFY (!VMRelease (pprc, dwRet, cPages));
            }
            dwRet = 0;
        }

        CELOG_VirtualAlloc(pprc, dwRet, dwAddr, cPages, fAllocType, fProtect);

    } else {
        *pdwErr = ERROR_INVALID_PARAMETER;
    }

    DEBUGMSG (ZONE_VIRTMEM||!dwRet, (L"DoVMAlloc - returns %8.8lx\r\n", dwRet));

    return (LPVOID) dwRet;
}

typedef struct {
    LPDWORD pPages;
    DWORD   dwPgProt;
    BOOL    fPhys;
    DWORD   dwPagerType;
} VSPStruct, *PVSPStruct;

static BOOL EnumSetPages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVSPStruct pvsps = (PVSPStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    DWORD dwPFN = pvsps->pPages[0];

    if (!dwEntry || IsPageCommitted (dwEntry)) {
        return FALSE;
    }
    if (!pvsps->fPhys) {
        dwPFN = GetPFN ((LPVOID)dwPFN);
    }
    if (!DupPhysPage (dwPFN)) {
        return FALSE;
    }
    pvsps->dwPagerType = GetPagerType (dwEntry);
    pvsps->pPages ++;
    *pdwEntry = MakeCommittedEntry (dwPFN, pvsps->dwPgProt);
    return TRUE;
}

//
// VMSetPages: (internal function) map the address from dwAddr of cPages pages, to the pages specified
//             in array pointed by pPages. The pages are treated as physical page number if PAGE_PHYSICAL
//             is specified in fProtect. Otherwise, it must be page aligned
//
BOOL VMSetPages (PPROCESS pprc, DWORD dwAddr, LPDWORD pPages, DWORD cPages, DWORD fProtect)
{
    PPAGEDIRECTORY ppdir = LockVM (pprc);
    BOOL fPhys = fProtect & PAGE_PHYSICAL;
    BOOL fRet  = (ppdir != NULL);
    fProtect &= ~PAGE_PHYSICAL;

    DEBUGCHK (pPages && cPages && IsValidProtect (fProtect) && !(VM_PAGE_OFST_MASK & dwAddr));

    if (fRet) {
        VSPStruct vsps = { pPages, PageParamFormProtect (fProtect, dwAddr), fPhys, 0 };

        // Calling DupPhysPage requires holding PhysCS
        LockPhysMem ();
        fRet = Enumerate2ndPT (ppdir, dwAddr, cPages, FALSE, EnumSetPages, &vsps);
        UnlockPhysMem ();

        UnlockVM (pprc);

        // on error, decommit the pages we committed
        if (!fRet && (cPages = vsps.pPages - pPages)) {
            VERIFY (VMDecommit (pprc, (LPVOID) dwAddr, cPages << VM_PAGE_SHIFT, vsps.dwPagerType));
        }

    }

    return fRet;
}


typedef struct _VCPhysStruct {
    DWORD dwPfn;        // current phys page number
    DWORD dwPgProt;      // page attributes
} VCPhysStruct, *PVCPhysStruct;

//-----------------------------------------------------------------------------------------------------------------
//
//  VCPhysPages: 2nd level page table enumeration funciton, VirtualCopy physical pages
//
static BOOL VCPhysPages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVCPhysStruct pvcps = (PVCPhysStruct) pEnumData;
    *pdwEntry = MakeCommittedEntry (pvcps->dwPfn, pvcps->dwPgProt);
    pvcps->dwPfn = NextPFN (pvcps->dwPfn);
    return TRUE;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMCopyPhysical: VirtualCopy from physical pages.
//
static BOOL
VMCopyPhysical(
    PPROCESS pprc,              // the destination process
    DWORD dwAddr,               // destination address
    DWORD dwPfn,                // physical page number
    DWORD cPages,               // # of pages
    DWORD dwPgProt              // protection
    )
{
    DWORD cPageReserved;
    BOOL  fRet = FALSE;

    DEBUGCHK (dwAddr);
    DEBUGCHK ((int) cPages > 0);
    DEBUGCHK (!(dwAddr & VM_PAGE_OFST_MASK));

    DEBUGMSG (ZONE_VIRTMEM, (L"VMCopyPhysical - process id: %8.8lx, dwAddr %8.8lx, dwPfn = %8.8lx cPages = 0x%x\r\n",
        pprc->dwId, dwAddr, dwPfn, cPages));

    if (LockVM (pprc)) {

        if (VMScan (pprc, dwAddr, cPages, &cPageReserved, 0)
            && (cPageReserved == cPages)) {

            VCPhysStruct vcps = { dwPfn, dwPgProt };

            fRet = Enumerate2ndPT (pprc->ppdir, dwAddr, cPages, 0, VCPhysPages, &vcps);
        }

        UnlockVM (pprc);
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"VMCopyPhysical returns 0x%x\r\n", fRet));

    return fRet;
}

typedef struct _VCVirtStruct {
    PPAGEDIRECTORY ppdir;   // 1st level page table
    PPAGETABLE pptbl;       // 2nd level page table
    DWORD idxdir;           // index to 1st level pt
    DWORD idx2nd;           // index to 2nd level pt
    DWORD dwPgProt;         // page attributes
    DWORD dwPagerType;
} VCVirtStruct, *PVCVirtStruct;

//-----------------------------------------------------------------------------------------------------------------
//
//  VCVirtPages: 2nd level page table enumeration funciton, VirtualCopy virtual pages
//
static BOOL VCVirtPages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    BOOL fRet = !IsPageCommitted (*pdwEntry);

    if (fRet) {
        PVCVirtStruct pvcvs = (PVCVirtStruct) pEnumData;
        DWORD dwPfn = PFNfromEntry (pvcvs->pptbl->pte[pvcvs->idx2nd]);

        pvcvs->dwPagerType = GetPagerType (*pdwEntry);

        if (fRet = DupPhysPage (dwPfn)) {

            *pdwEntry = MakeCommittedEntry (dwPfn, pvcvs->dwPgProt);

            if (VM_NUM_PT_ENTRIES == ++ pvcvs->idx2nd) {
                pvcvs->idx2nd = 0;
                pvcvs->idxdir = NextPDEntry (pvcvs->idxdir);
                pvcvs->pptbl = Entry2PTBL (pvcvs->ppdir->pte[pvcvs->idxdir]);
            }
        }
    }

    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMCopyVirtual: VirtualCopy between 2 Virtual addresses
//
static BOOL
VMCopyVirtual(
    PPROCESS pprcDest,      // the destination process
    DWORD dwDestAddr,       // destination address
    PPROCESS pprcSrc,       // the source process
    DWORD dwSrcAddr,        // source address, NULL if PAGE_PHYSICAL
    DWORD cPages,           // # of pages
    DWORD dwPgProt,         // protection
    LPDWORD pdwErr          // error code
    )
{
    DWORD cPageReserved;
    BOOL fRet = FALSE;

    DEBUGCHK (dwDestAddr);
    DEBUGCHK ((int) cPages > 0);
    DEBUGCHK (!(dwDestAddr & VM_PAGE_OFST_MASK));
    DEBUGCHK (!(dwSrcAddr & VM_PAGE_OFST_MASK));

    // default to invalid parameter
    *pdwErr = ERROR_INVALID_PARAMETER;

    if (!pprcSrc)
        pprcSrc = pprcDest;

    DEBUGMSG (ZONE_VIRTMEM, (L"VMCopyVirtual - dst process-id: %8.8lx, src process-id: %8.8lx, dwDestAddr %8.8lx, dwSrcAddr = %8.8lx cPages = 0x%x\r\n",
        pprcDest->dwId, pprcSrc->dwId, dwDestAddr, dwSrcAddr, cPages));

    // locking 2 proceses's VM needs to take care of CS ordering, or we might
    // run into dead lock.
    if (Lock2VM (pprcDest, pprcSrc)) {

        PPAGEDIRECTORY ppdir = pprcSrc->ppdir;
        DWORD         idxdir = VA2PDIDX(dwSrcAddr);
        PPAGETABLE     pptbl = Entry2PTBL (ppdir->pte[idxdir]);
        VCVirtStruct    vcvs = { ppdir, pptbl, idxdir, VA2PT2ND(dwSrcAddr), dwPgProt, 0 };

        // verify parameters - destination, must be all reserved, source - must be all committed
        if (VMScan (pprcDest, dwDestAddr, cPages, &cPageReserved, 0)
            && (cPageReserved == cPages)
            && VMScan (pprcSrc, dwSrcAddr, cPages, &cPageReserved, 0)
            && (0 == cPageReserved)) {

            DEBUGCHK (pptbl);

            LockPhysMem ();
            fRet = Enumerate2ndPT (pprcDest->ppdir, dwDestAddr, cPages, 0, VCVirtPages, &vcvs);
            UnlockPhysMem ();

            *pdwErr = fRet? 0 : ERROR_OUTOFMEMORY;

        }
        DEBUGMSG ((ERROR_OUTOFMEMORY == *pdwErr) || !fRet,
            (L"VMCopyVirtual Failed, cPages = %8.8lx, cpReserved = %8.8lx\r\n", cPages, cPageReserved));

        UnlockVM (pprcDest);
        UnlockVM (pprcSrc);

        // if fail due to OOM, decommit the pages
        if (ERROR_OUTOFMEMORY == *pdwErr) {
            VERIFY (VMDecommit (pprcDest, (LPVOID) dwDestAddr, cPages << VM_PAGE_SHIFT, vcvs.dwPagerType));
        }
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"VMCopyVirtual - returns 0x%x\r\n", fRet));

    return fRet;
}

//
// PageAutoCommit: auto-commit a page.
// NOTE: PageAutoCommit returns PAGEIN_FAILURE FOR ALREADY COMMITTED PAGE.
//       The reason being that it'll have to be a PERMISSION FAULT if
//       that happen.
//
static DWORD PageAutoCommit (PPROCESS pprc, DWORD dwAddr, BOOL fWrite)
{
    DWORD dwRet = PAGEIN_RETRY;
    PPAGEDIRECTORY ppdir = LockVM (pprc);

    DEBUGMSG (ZONE_VIRTMEM, (L"Auto-Commiting Page %8.8lx\r\n", dwAddr));
    if (ppdir) {
        DWORD idxdir = VA2PDIDX (dwAddr);
        DWORD idx2nd = VA2PT2ND (dwAddr);
        PPAGETABLE pptbl = Entry2PTBL (ppdir->pte[idxdir]);

        if (IsPageCommitted (pptbl->pte[idx2nd])) {
            // already commited
            dwRet = IsPageWritable (pptbl->pte[idx2nd])? PAGEIN_SUCCESS : PAGEIN_FAILURE;

        } else  if (HoldPages (1, FALSE)) {
            // newly commited, update pagetable entry
            pptbl->pte[idx2nd] = MakeWritableEntry (GetHeldPage (PM_PT_ZEROED), dwAddr);
#ifdef ARM
            ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
            dwRet = PAGEIN_SUCCESS;
        } else {
            // low on memory, retry (do nothing)
        }
        UnlockVM (pprc);
    } else {
        dwRet = PAGEIN_FAILURE;
        DEBUGCHK (0);   // should've never happen.
    }
    return dwRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMProcessPageFault: Page fault handler.
//
BOOL VMProcessPageFault (PPROCESS pprc, DWORD dwAddr, BOOL fWrite)
{
    DWORD idxdir = VA2PDIDX (dwAddr);
    DWORD idx2nd = VA2PT2ND (dwAddr);
    BOOL  fRet = FALSE;
    PPAGETABLE pptbl;

    // use kernel as the process to page if above shared heap.
    if (dwAddr > VM_SHARED_HEAP_BASE) {
        pprc = g_pprcNK;
    }
    pptbl = Entry2PTBL (pprc->ppdir->pte[idxdir]);

    DEBUGMSG (ZONE_VIRTMEM|ZONE_PAGING, (L"VMProcessPageFault: pprc = %8.8lx, dwAddr = %8.8lx, fWrite = %8.8lx\r\n",
        pprc, dwAddr, fWrite));
    CELOG_PageFault(pprc, dwAddr, TRUE, fWrite);

    // check for page faults in power handler
    if (IsInPwrHdlr()) {
        NKDbgPrintfW(_T("VMProcessPageFault Error: Page fault occurred while in power handler! Address = 0x%08x\r\n"), dwAddr);

        // Always return FALSE in debug images to catch the exception in the proper place.
#ifndef DEBUG
        if (gdwFailPowerPaging == 1)
#endif
        {
            // Fail this call so that an exception will be raised at
            // the address of the code which is being paged-in.
            return FALSE;
        }
    }

    if (pptbl) {
        DWORD    dwEntry = pptbl->pte[idx2nd];

        // special case for committed page - only mapfile handles write to r/o page.
        PFNPager pfnPager = IsPageCommitted (dwEntry)? PageFromMapper : g_vmPagers[GetPagerType(dwEntry)];
        DWORD    dwPgRslt;
#ifdef DEBUG
        DWORD    dwRetries = 0;
#endif

        DEBUGMSG (ZONE_VIRTMEM|ZONE_PAGING, (L"VMProcessPageFault: PageType = %d, PageFunc = %8.8lx\r\n",
            GetPagerType(pptbl->pte[idx2nd]), pfnPager));

        while (TRUE) {

            fRet = IsPageWritable (dwEntry)                     // the page is writable
                   || (!fWrite && IsPageReadable (dwEntry));    // or we're requesting read, and the page is readable

            if (fRet) {
                break;
            }

            dwPgRslt = pfnPager (pprc, dwAddr, fWrite);

            if (PAGEIN_RETRY != dwPgRslt) {
                fRet = (PAGEIN_SUCCESS == dwPgRslt);
                break;
            }

            DEBUGMSG (ZONE_PAGING|ZONE_PAGING, (TEXT("Page function returned 'retry'\r\n")));
            Sleep(250); // arbitrary number, should be > 0 but pretty small
            // DEBUGMSG every 5 secs to help catch hangs
            DEBUGMSG (0 == (dwRetries = (dwRetries+1)%20), (TEXT("VMProcessPageFault: still looping\r\n")));

            dwEntry = pptbl->pte[idx2nd];
        }

        // handle guard pages
        if (IsGuardPage (dwEntry)) {
            InterlockedCompareExchange (&pptbl->pte[idx2nd], CommitGuardPage (dwEntry), dwEntry);
#ifdef ARM
            ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
        }
    }

    DEBUGMSG (ZONE_VIRTMEM|ZONE_PAGING, (L"VMProcessPageFault: returns = %d\r\n", fRet));

    // notify kernel debugger if page-in successfully.
    if (fRet) {
        (*HDPageIn) (PAGEALIGN_DOWN(dwAddr), fWrite);
    }

    CELOG_PageFault(pprc, dwAddr, FALSE, fRet);

    return fRet;
}


//-----------------------------------------------------------------------------------------------------------------
//
// KC_VMDemandCommit: Demand commit a stack page for exception handler.
// NOTE: This function is called within KCall.
//
DWORD KC_VMDemandCommit (DWORD dwAddr)
{
    DWORD dwRet = DCMT_OLD;

    if (!IsInKVM (dwAddr)) {
        DEBUGCHK (dwAddr > VM_KMODE_BASE);
        return (dwAddr > VM_KMODE_BASE)? DCMT_OLD : DCMT_FAILED;
    }

    if (IsInKVM (dwAddr)) {
        PPAGETABLE pptbl = Entry2PTBL (g_ppdirNK->pte[VA2PDIDX (dwAddr)]);
        DWORD idx2nd = VA2PT2ND (dwAddr);

        DEBUGMSG (ZONE_VIRTMEM, (L"KC_VMDemandCommit: pptbl = %8.8lx, idx2nd = %8.8lx\r\n", pptbl, idx2nd));

        DEBUGCHK (pptbl);   // we will always call KC_VMDemandCommit with a valid secure stack
        if (!IsPageWritable (pptbl->pte[idx2nd])) {
            LPVOID pPage = GrabOnePage (PM_PT_ANY);

            DEBUGMSG (ZONE_VIRTMEM, (L"KC_VMDemandCommit: Committing page %8.8lx\r\n", dwAddr));
            DEBUGCHK (GetPagerType (pptbl->pte[idx2nd]) == VM_PAGER_AUTO);

            if (pPage) {
                pptbl->pte[idx2nd] = MakeWritableEntry (GetPFN (pPage), dwAddr);
#ifdef ARM
                ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
                dwRet = DCMT_NEW;
            } else {
                RETAILMSG (1, (L"!!!!KC_VMDemandCommit: Completely OUT OF MEMORY!!!!\r\n"));
                dwRet = DCMT_FAILED;
            }
        }
    }

    return dwRet;
}


typedef struct _LockEnumStruct {
    DWORD       cPagesNeeded;       // how many page we need to page in
    BOOL        fWrite;             // write access?
} LockEnumStruct, *PLockEnumStruct;


static BOOL CheckLockable (LPDWORD pdwEntry, LPVOID pEnumData)
{
    DWORD dwEntry = *pdwEntry;
    PLockEnumStruct ples = (PLockEnumStruct) pEnumData;
    BOOL  fRet;

    if (IsPageCommitted (dwEntry)) {
        fRet = ples->fWrite? IsPageWritable (dwEntry) : IsPageReadable (dwEntry);
    } else {
        fRet = (VM_PAGER_NONE != GetPagerType (dwEntry));
        ples->cPagesNeeded ++;
    }
    return fRet;
}

typedef struct _PagingEnumStruct {
    PPROCESS pprc;
    DWORD    dwAddr;
    LPDWORD  pPFNs;
    BOOL     fWrite;
} PagingEnumStruct, *PPagingEnumStruct;

static BOOL PageInOnePage (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PPagingEnumStruct ppe = (PPagingEnumStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    BOOL fRet = IsPageWritable (dwEntry)                     // the page is writable
           || (!ppe->fWrite && IsPageReadable (dwEntry));    // or we're requesting read, and the page is readable

    if (!fRet
        && !VMProcessPageFault (ppe->pprc, ppe->dwAddr, ppe->fWrite)) {
        DEBUGMSG (ZONE_VIRTMEM, (L"Paging in %8.8lx for process %8.8lx failed\r\n", ppe->dwAddr, ppe->pprc));
        return FALSE;
    }
    DEBUGCHK (IsPageCommitted (*pdwEntry));

    ppe->dwAddr += VM_PAGE_SIZE;

    // update pfn array if requested
    if (ppe->pPFNs) {
        ppe->pPFNs[0] = PFNfromEntry (*pdwEntry);
        ppe->pPFNs ++;
    }

    return TRUE;
}

//
// using the simplest O(n) algorithm here.
//
//  We're assuming that the locked page list will not grow extensively - only drivers
//  doing DMA should be locking pages for long period of time. All other locking should
//  be temporary and would be unlocked soon.
//
//  We will have to revisit this if we found that the locked page list grows beyond control
//  for it'll affect the perf of VM allocation if the list gets too long.
//

//
// locking/unlocking VM pages.
//
BOOL VMAddToLockPageList     (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    PLOCKPAGELIST plp = (PLOCKPAGELIST) AllocMem (HEAP_LOCKPAGELIST);
    DEBUGCHK (!(dwAddr & VM_PAGE_OFST_MASK));

    if (plp) {
        plp->dwAddr = dwAddr;
        plp->cbSize = (cPages << VM_PAGE_SHIFT);
        plp->pNext  = pprc->pLockList;
        pprc->pLockList = plp;
    }
    return TRUE;
}

BOOL VMRemoveFromLockPageList   (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    DWORD cbSize = cPages << VM_PAGE_SHIFT;
    PLOCKPAGELIST *pplp = &pprc->pLockList, plp;

    for (pplp = &pprc->pLockList; plp = *pplp; pplp = &plp->pNext) {
        if (   (dwAddr == plp->dwAddr)
            && (cbSize == plp->cbSize)) {
            // found, remove from list
            *pplp = plp->pNext;
            FreeMem (plp, HEAP_LOCKPAGELIST);
            return TRUE;
        }
    }


    return FALSE;
}

DWORD dwMaxLockListIters;

BOOL VMIsPagesLocked (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    PLOCKPAGELIST plp;
    DWORD cbSize = cPages << VM_PAGE_SHIFT;
#ifdef VM_DEBUG
    DWORD nIters = 0;
#endif
    for (plp = pprc->pLockList; plp; plp = plp->pNext) {
#ifdef VM_DEBUG
        nIters ++;
#endif
        if ((dwAddr < plp->dwAddr + plp->cbSize)
            && (dwAddr + cbSize > plp->dwAddr)) {
            return TRUE;
        }
    }

#ifdef VM_DEBUG
    if (dwMaxLockListIters < nIters) {
        dwMaxLockListIters = nIters;
    }
#endif

    return FALSE;
}

void VMFreeLockPageList (PPROCESS pprc)
{
    DEBUGCHK (pprc != g_pprcNK);
    if (LockVM (pprc)) {
        PLOCKPAGELIST plp;
        pprc->bState = PROCESS_STATE_NO_LOCK_PAGES;
        while (plp = pprc->pLockList) {
            DEBUGMSG (1, (L"Process'%s' exited with address 0x%8.8lx of size %0x%x still locked\r\n",
                pprc->lpszProcName, plp->dwAddr, plp->cbSize));
            DEBUGCHK (0);
            pprc->pLockList = plp->pNext;
            FreeMem (plp, HEAP_LOCKPAGELIST);
        }
        UnlockVM (pprc);
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// DoVMLockPages: worker function to lock VM pages.
//      returns 0 if success, error code if failed
//
static DWORD DoVMLockPages (PPROCESS pprc, DWORD dwAddr, DWORD cPages, LPDWORD pPFNs, DWORD fOptions)
{
    PPAGEDIRECTORY ppdir;
    DWORD dwErr = 0;

    if (dwAddr >= VM_SHARED_HEAP_BASE) {
        pprc = g_pprcNK;
    }

    if (ppdir = LockVM (pprc)) {

        DEBUGCHK (IsLockPageAllowed (pprc));

        // add the pages to locked VM list, to prevent them from being paged out
        if (   !IsLockPageAllowed (pprc)                                // lock pages not allowed
            || !VMAddToLockPageList (pprc, dwAddr, cPages)) {           // failed to add to locked page list
            DEBUGMSG (ZONE_VIRTMEM, (L"DoVMLockPages: failed 2\r\n"));
            dwErr = ERROR_NOT_ENOUGH_MEMORY;

        }

        // release VM lock before we start paging in
        UnlockVM (pprc);

        if (!dwErr) {

            // page in the pages
            // NOTE: We *cannot* lock VM here for it will introduce deadlock if pager is invoked. However, the
            //       enumeration here is safe because we've already added the pages to the 'locked page list',
            //       thus these pages cannot be paged out, and page tables cannot be destroyed.
            //       When we encounter pages that needs to be paged in, the pager functions handles VM
            //       locking/unlocking themselves.
            //
            PagingEnumStruct pe = { pprc, dwAddr, pPFNs, fOptions & VM_LF_WRITE };

            // page in all the pages
            if (!Enumerate2ndPT (ppdir, dwAddr, cPages, 0, PageInOnePage, &pe)) {
                DEBUGMSG (ZONE_VIRTMEM, (L"Unable to Page in from %8.8lx for %d Pages, fWrite = %d\r\n", dwAddr, cPages, fOptions & VM_LF_WRITE));
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            }

            // unlock the pages if query only or error occurs
            if (dwErr || (VM_LF_QUERY_ONLY & fOptions)) {
                VERIFY (LockVM (pprc));
                VERIFY (VMRemoveFromLockPageList (pprc, dwAddr, cPages));
                UnlockVM (pprc);
            }
        }

    } else {
        dwErr = ERROR_INVALID_PARAMETER;
    }

    return dwErr;
}

//-----------------------------------------------------------------------------------------------------------------
//
// DoVMUnlockPages: worker function to unlock VM.
//      returns 0 if success, error code if failed
//
static DWORD DoVMUnlockPages (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    DWORD dwErr = ERROR_INVALID_PARAMETER;
    if (dwAddr >= VM_SHARED_HEAP_BASE) {
        pprc = g_pprcNK;
    }
    if (LockVM (pprc)) {
        if (VMRemoveFromLockPageList (pprc, dwAddr, cPages)) {
            dwErr = 0;
        }
        UnlockVM (pprc);
    }
    return dwErr;
}


//-----------------------------------------------------------------------------------------------------------------
//
//  DecommitPages: 2nd level page table enumeration function, Decommit virtual pages
//      pEnumData: (DWORD) the pager type, or (VM_MAX_PAGER+1) for release.
//                 May also include VM_PAGER_POOL flag for releasing to pool.
//
static BOOL DecommitPages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    DWORD dwEntry = *pdwEntry;
    DWORD dwPagerType = ((DWORD) pEnumData) & ~VM_PAGER_POOL;

    // decommit the page if committed
    if (IsPageCommitted (dwEntry)) {
        DWORD dwPFN = PFNfromEntry (dwEntry);

        if ((((DWORD) pEnumData) & VM_PAGER_POOL)) {
            // Return to the proper page pool
            DEBUGCHK ((VM_PAGER_LOADER == dwPagerType) || (VM_PAGER_MAPPER == dwPagerType));
            PGPOOLFreePage ((VM_PAGER_LOADER == dwPagerType) ? g_pLoaderPool : g_pFilePool,
                            Pfn2Virt (dwPFN));
        } else {
            FreePhysPage (dwPFN);
        }
        *pdwEntry = MakeReservedEntry (dwPagerType);
    }

    // make entry free if releasing the region
    if (VM_MAX_PAGER < dwPagerType) {
        *pdwEntry = VM_FREE_PAGE;
    }

    return TRUE;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMDecommit: worker function to decommit pages for VirtualFree.
//      return 0 if succeed, error code if failed
//
BOOL VMDecommit (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD dwPagerType)
{
    DWORD dwAddr = (DWORD) pAddr;
    DWORD dwErr = ERROR_INVALID_PARAMETER;
    DWORD dwOfst = dwAddr & VM_PAGE_OFST_MASK;
    DWORD cPages = PAGECOUNT (cbSize + dwOfst);

    DEBUGCHK ((int) cPages > 0);
    dwAddr -= dwOfst;

    DEBUGMSG (ZONE_VIRTMEM, (L"VMDecommit - Decommitting from %8.8lx, for 0x%x pages\r\n", dwAddr, cPages));
    if (LockVM (pprc)) {

        if (!VMIsPagesLocked (pprc, dwAddr, cPages)) {

            // write-back all dirty pages
            OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_WRITEBACK);

            VERIFY (Enumerate2ndPT (pprc->ppdir, dwAddr, cPages, 0, DecommitPages, (LPVOID) dwPagerType));

            InvalidatePages (pprc, dwAddr, cPages);
            dwErr = 0;

            CELOG_VirtualFree(pprc, dwAddr, cPages, MEM_DECOMMIT);
        }

        UnlockVM (pprc);
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }
    DEBUGMSG (ZONE_VIRTMEM, (L"VMDecommit - returns dwErr = 0x%x\r\n", dwErr));

    return !dwErr;
}

#define VM_MAX_PAGES       (0x80000000 >> VM_PAGE_SHIFT)


//-----------------------------------------------------------------------------------------------------------------
//
// VMRelease: worker function to release VM reservation.
//      return 0 if succeed, error code if failed
//
static DWORD VMRelease (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    PPAGEDIRECTORY ppdir;
    DWORD dwErr = ERROR_INVALID_PARAMETER;

    DEBUGCHK (!(dwAddr & VM_BLOCK_OFST_MASK));
    DEBUGCHK (cPages);

    if (ppdir = LockVM (pprc)) {

        if (VMIsPagesLocked (pprc, dwAddr, cPages)) {
            DEBUGMSG (1, (L"VMRelease: Cannot release VM from dwAddr = %8.8lx of %8.8lx pages because the pages are locked\r\n", dwAddr, cPages));

        } else {
            LPDWORD pdwBaseFree = &pprc->vaFree;

            DEBUGMSG (ZONE_VIRTMEM, (L"VMRelease: release VM from dwAddr = %8.8lx of %8.8lx pages\r\n", dwAddr, cPages));

            // write-back all dirty pages
            OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_WRITEBACK);

            // pass (VM_MAX_PAGER + 1) to DecommitPages will release the region
            VERIFY (Enumerate2ndPT (ppdir, dwAddr, cPages, 0, DecommitPages, (LPVOID) (VM_MAX_PAGER + 1)));
            InvalidatePages (pprc, dwAddr, cPages);

            if (pprc == g_pprcNK) {
                // NK - take care of special ranges
                if (IsInSharedHeap (dwAddr)) {
                    pdwBaseFree = &g_vaSharedHeapFree;
                } else if (IsInRAMMapSection (dwAddr)) {
                    pdwBaseFree = &g_vaRAMMapFree;
                } else if (!IsInKVM (dwAddr)) {
                    DEBUGCHK (dwAddr >= VM_DLL_BASE);
                    pdwBaseFree = &g_vaDllFree;
                }
            }

            if (*pdwBaseFree > dwAddr) {
                *pdwBaseFree = dwAddr;
            }
            dwErr = 0;

            CELOG_VirtualFree(pprc, dwAddr, cPages, MEM_RELEASE);
        }
        UnlockVM (pprc);
    }

    return dwErr;
}

//-----------------------------------------------------------------------------------------------------------------
//
// ChangeProtection: 2nd level PT enumeration funciton. change paget table entry protection
//          (DWORD) pEnumData  is the new permission
//
static BOOL ChangeProtection (LPDWORD pdwEntry, LPVOID pEnumData)
{
    *pdwEntry = MakeCommittedEntry (PFNfromEntry (*pdwEntry), (DWORD) pEnumData);

    return TRUE;
}


//-----------------------------------------------------------------------------------------------------------------
//
// DoVMProtect: worker function to change VM protection.
//      return 0 if succeed, error code if failed
//
static DWORD DoVMProtect (PPROCESS pprc, DWORD dwAddr, DWORD cPages, DWORD fNewProtect, LPDWORD pdwOldProtect)
{
    DWORD dwErr = 0;
    DWORD cpNeeded = 0;         // # of uncommited pages

    if (LockVM (pprc)) {

        if (VMIsPagesLocked (pprc, dwAddr, cPages)) {
            dwErr = ERROR_SHARING_VIOLATION;

        } else if (!VMScan (pprc, dwAddr, cPages, &cpNeeded, fNewProtect & VM_READWRITE_PROT)  // pages in the same reserved region?
            || cpNeeded) {                                                              // all page committed?
            dwErr = ERROR_INVALID_PARAMETER;

        } else {
            PPAGEDIRECTORY ppdir = pprc->ppdir;

            __try {
                if (pdwOldProtect) {
                    PPAGETABLE pptbl = Entry2PTBL (ppdir->pte[VA2PDIDX (dwAddr)]);
                    DEBUGCHK (pptbl);
                    *pdwOldProtect = ProtectFromEntry (pptbl->pte[VA2PT2ND(dwAddr)]);

                }
                VERIFY (Enumerate2ndPT (ppdir, dwAddr, cPages, 0, ChangeProtection, (LPVOID) PageParamFormProtect (fNewProtect, dwAddr)));

                InvalidatePages (pprc, dwAddr, cPages);

            } __except (EXCEPTION_EXECUTE_HANDLER) {
                dwErr = ERROR_INVALID_PARAMETER;
            }
        }

        UnlockVM (pprc);
    } else {
        dwErr = ERROR_INVALID_PARAMETER;
    }
    return dwErr;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMFastAllocCopyToKernel: (internal only) reserve VM and VirtualCopy from source proceess/address.
//              Source is always pVMProc, Destination process is always kernel, and always R/O
//
static LPVOID
VMFastAllocCopyToKernel (
    LPCVOID  pAddr,                 // address to be copy from
    DWORD    cbSize                 // size of the ptr
    )
{
    DWORD dwOfst = (DWORD) pAddr & VM_PAGE_OFST_MASK;
    DWORD dwAddr = (DWORD) pAddr & ~VM_PAGE_OFST_MASK;
    DWORD dwBlkOfst = ((DWORD)pAddr & VM_BLOCK_OFST_MASK);
    LPBYTE pDst = VMReserve (g_pprcNK, cbSize + dwBlkOfst, 0, 0);
    DWORD dwErr = 0;

    if (!pDst) {
        dwErr  = ERROR_NOT_ENOUGH_MEMORY;

    } else {
        DWORD cPages;
        DWORD _pPFNs[64];   // local buffer up to 64 pages
        LPDWORD pPFNs = _pPFNs;

        pDst   += dwBlkOfst - dwOfst;       // page align destination
        cbSize += dwOfst;                   // size, starting from paged aligned address
        cPages = PAGECOUNT (cbSize);

        if ((cPages > 64) && !(pPFNs = NKmalloc (cPages * sizeof (DWORD)))) {
            dwErr = ERROR_NOT_ENOUGH_MEMORY;

        } else if (!(dwErr = DoVMLockPages (pVMProc, dwAddr, cPages, pPFNs, VM_LF_READ))) {

            if (!VMSetPages (g_pprcNK, (DWORD) pDst, pPFNs, cPages, PAGE_READONLY|PAGE_PHYSICAL)) {
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            }

            VERIFY (!DoVMUnlockPages (pVMProc, dwAddr, cPages));
        }

        if (_pPFNs != pPFNs) {
            NKfree (pPFNs);
        }

        if (dwErr) {
            VERIFY (VMFreeAndRelease (g_pprcNK, pDst, cbSize));
        } else {
            pDst += dwOfst;
        }
    }

    if (dwErr) {
        NKSetLastError (dwErr);
    }

    // NOTE: NO CACHE FLUSH HERE EVEN FOR VIVT CACHE. CALLER IS RESPONSIBLE FOR FLUSHING CACHE
    return dwErr? NULL : pDst;

}



#define LOCAL_COPY_SIZE      2*MAX_PATH // cico optimization for strings and small buffers

//-----------------------------------------------------------------------------------------------------------------
//
// VMReadProcessMemory: read process memory.
//      return 0 if succeed, error code if failed
//
DWORD VMReadProcessMemory (PPROCESS pprc, LPCVOID pAddr, LPVOID pBuf, DWORD cbSize)
{
    DWORD    dwErr     = 0;
    PTHREAD  pth       = pCurThread;
    DWORD    dwSaveKrn = pth->tlsPtr[TLSSLOT_KERNEL];

    pth->tlsPtr[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG;

    if ((int) cbSize <= 0) {
        dwErr = cbSize? ERROR_INVALID_PARAMETER : 0;

    } else if ((DWORD) pAddr >= VM_SHARED_HEAP_BASE) {

        // reading kernel address, direct copy
        if (!CeSafeCopyMemory (pBuf, pAddr, cbSize)) {
            dwErr = ERROR_INVALID_PARAMETER;
        }

    } else {

        // reading user-mode address. Need to switch VM regardless, for we can be called from
        // a kernel thread, which doesn't have any VM affinity.
        PPROCESS pprcOldVM = SwitchVM (pprc);

        //
        // Active process switch is needed because we don't flush cache/TLB when CPU supports ASID.
        // In which case, we can end up leaving wrong entry in TLB if we don't switch active process.
        //
        PPROCESS pprcOldActv = SwitchActiveProcess (pprc);

        if (((DWORD) pBuf >= VM_SHARED_HEAP_BASE) || (pprc == pprcOldVM)) {
            // pBuf is accesible after VM switch, just copy
            if (!CeSafeCopyMemory (pBuf, pAddr, cbSize)) {
                dwErr = ERROR_INVALID_PARAMETER;
            }
        } else {
            // pBuf is not accesible after VM switch.
            BYTE    _localCopy[LOCAL_COPY_SIZE];
            LPVOID  pLocalCopy = _localCopy;

            if (cbSize <= LOCAL_COPY_SIZE) {
                // small size - use CICO
                if (!CeSafeCopyMemory (pLocalCopy, pAddr, cbSize)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
            } else if (!(pLocalCopy = VMFastAllocCopyToKernel (pAddr, cbSize))) {
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            }

            if (!dwErr) {
                DEBUGCHK (pprcOldVM);   // kernel thread calling ReadProcesssMemory with a user buffer. Should never happen.

                // Note: the VM switch below will flush cache on VIVT. So no explicit cache flush
                //       is needed here.

                SwitchActiveProcess (pprcOldActv);  // switch back to original active process
                SwitchVM (pprcOldVM);               // switch back to orignial VM
                if (!CeSafeCopyMemory (pBuf, pLocalCopy, cbSize)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
            }

            if ((_localCopy != pLocalCopy) && pLocalCopy) {
                VERIFY (VMFreeAndRelease (g_pprcNK, pLocalCopy, cbSize));
            }
        }

        // we may end-up doing one extra switch here, but it's alright for the calls will be no-op.
        SwitchActiveProcess (pprcOldActv);
        SwitchVM (pprcOldVM);
    }

    pth->tlsPtr[TLSSLOT_KERNEL] = dwSaveKrn;

    return dwErr;
}

static BOOL ReplacePages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    DWORD dwEntry = *pdwEntry;
    BOOL fRet = IsPageCommitted (dwEntry);

    if (fRet) {
        PVCVirtStruct pvcvs = (PVCVirtStruct) pEnumData;
        DWORD dwPfn = PFNfromEntry (pvcvs->pptbl->pte[pvcvs->idx2nd]);

        EnterCriticalSection (&PhysCS);
        fRet = DupPhysPage (dwPfn);
        LeaveCriticalSection (&PhysCS);

        if (fRet) {
            *pdwEntry = MakeCommittedEntry (dwPfn, dwEntry & PG_PERMISSION_MASK);

            FreePhysPage (PFNfromEntry (dwEntry));

            if (VM_NUM_PT_ENTRIES == ++ pvcvs->idx2nd) {
                pvcvs->idx2nd = 0;
                pvcvs->idxdir = NextPDEntry (pvcvs->idxdir);
                pvcvs->pptbl  = Entry2PTBL (pvcvs->ppdir->pte[pvcvs->idxdir]);
            }
        }
    }

    return fRet;
}

//
// copy-on-write support only for user mode
// exe addresses or user mode dll address
//
static DWORD VMCopyOnWrite (PPROCESS pprc, LPVOID pAddr, LPCVOID pBuf, DWORD cbSize)
{
    DWORD dwErr = ERROR_INVALID_PARAMETER;
    DWORD dwAddr = (DWORD) pAddr;
    DWORD dwProt = 0;
    DWORD dwPageOfst = dwAddr & VM_PAGE_OFST_MASK;
    DWORD  cPages = PAGECOUNT  (cbSize + dwPageOfst);

    DEBUGCHK (pprc != g_pprcNK);                        // cannot be kernel
    DEBUGCHK (pprc == pVMProc);                         // pprc must be current VM
    DEBUGCHK ((DWORD) pBuf >= VM_SHARED_HEAP_BASE);     // pBuf must be global accessible area

    // EXE address (not in ROM)
    if ((dwAddr < (DWORD) pprc->BasePtr + pprc->e32.e32_vsize)
        && !(pprc->oe.filetype & FA_XIP)) {

        // change the protection to read-write
        if(VMProtect(pprc, pAddr, cbSize, PAGE_READWRITE, &dwProt)) {

            // update the code pages
            if (CeSafeCopyMemory(pAddr, pBuf, cbSize))
            {
                // flush the cache for the address range
                dwAddr -= dwPageOfst;
                OEMCacheRangeFlush ((LPVOID)dwAddr,  cPages << VM_PAGE_SHIFT, CACHE_SYNC_WRITEBACK|CACHE_SYNC_INSTRUCTIONS);
                dwErr = 0;
            }

            // revert back the permissions on the page
            // the following call will flush the tlb
            VMProtect(pprc, pAddr, cbSize, dwProt, NULL);
        }
    }

    // DLL address or failed to write exe address
    if (dwErr && (dwAddr > VM_DLL_BASE) && (dwAddr < VM_RAM_MAP_BASE)) {

        // try to do copy on write.
        DWORD dwBlkOfst  = dwAddr & VM_BLOCK_OFST_MASK;
        LPBYTE pBase = (LPBYTE) VMReserve (g_pprcNK, cbSize + dwBlkOfst, 0, 0);
        LPBYTE pBaseCopy = pBase + dwBlkOfst - dwPageOfst;

        if (!pBase || !VMCommit (g_pprcNK, pBaseCopy, cPages << VM_PAGE_SHIFT, PAGE_READWRITE, PM_PT_ANY)) {
            dwErr = ERROR_NOT_ENOUGH_MEMORY;

        } else {

            dwAddr -= dwPageOfst;       // page align start address

            // copy original content IN PAGES from target to the local copy.
            if (CeSafeCopyMemory (pBaseCopy, (LPCVOID) dwAddr, cPages << VM_PAGE_SHIFT) // copy original contents
                && CeSafeCopyMemory (pBaseCopy + dwPageOfst, pBuf, cbSize)              // copy the inteneded data to be written
                && LockVM (pprc)) {                                                     // process not being destroyed.

                VCVirtStruct   vcvs;

                // flush the cache for the address range
                OEMCacheRangeFlush ((LPVOID)dwAddr, cPages << VM_PAGE_SHIFT, CACHE_SYNC_WRITEBACK|CACHE_SYNC_INSTRUCTIONS);

                // setup arguments to replace pages
                vcvs.idxdir = VA2PDIDX (pBaseCopy);
                vcvs.idx2nd = VA2PT2ND (pBaseCopy);

                // lock kernel VM
                vcvs.ppdir  = LockVM (g_pprcNK);
                vcvs.pptbl  = Entry2PTBL (g_ppdirNK->pte[vcvs.idxdir]);

                // replace the destination pages,
                dwErr = Enumerate2ndPT (pprc->ppdir, dwAddr, cPages, 0, ReplacePages, &vcvs)
                                ? 0
                                : ERROR_INVALID_PARAMETER;

                // this will flush the tlb
                InvalidatePages (pprc, dwAddr, cPages);

                UnlockVM (g_pprcNK);
                UnlockVM (pprc);

            }
        }

        if (pBase) {
            VERIFY (VMFreeAndRelease (g_pprcNK, pBase, cbSize + dwBlkOfst));
        }
    }

    return dwErr;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMWriteProcessMemory: write process memory.
//      return 0 if succeed, error code if failed
//
DWORD VMWriteProcessMemory (PPROCESS pprc, LPVOID pAddr, LPCVOID pBuf, DWORD cbSize)
{
    DWORD    dwErr     = 0;
    PTHREAD  pth       = pCurThread;
    DWORD    dwSaveKrn = pth->tlsPtr[TLSSLOT_KERNEL];

    pth->tlsPtr[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG;

    if ((int) cbSize <= 0) {
        dwErr = cbSize? ERROR_INVALID_PARAMETER : 0;

    } else if ((DWORD) pAddr >= VM_SHARED_HEAP_BASE) {

        // writing kernel address, direct copy
        if (!CeSafeCopyMemory (pAddr, pBuf, cbSize)) {
            dwErr = ERROR_INVALID_PARAMETER;
        }

    } else {

        // writing to user mode address.
        BYTE    _localCopy[LOCAL_COPY_SIZE];
        LPVOID  pLocalCopy = _localCopy;

        // kernel threads should've never call WriteProcessMemory with a user buffer as source.
        DEBUGCHK (pth->pprcVM || ((DWORD) pBuf >= VM_SHARED_HEAP_BASE));

        if (((DWORD) pBuf < VM_SHARED_HEAP_BASE) && (pprc != pVMProc)) {

            // pBuf not accessible after VM switch. make a local copy
            if (cbSize <= LOCAL_COPY_SIZE) {
                // small size - use CICO
                if (!CeSafeCopyMemory (pLocalCopy, pBuf, cbSize)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
            } else if (!(pLocalCopy = VMFastAllocCopyToKernel (pBuf, cbSize))) {
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            }

            // switch the source to the local copy
            pBuf = pLocalCopy;
        }

        if (!dwErr) {

            // VM Switch here ensure that we're writing the cache back before the copy for VIVT cache
            PPROCESS pprcOldVM   = SwitchVM (pprc);
            PPROCESS pprcOldActv = SwitchActiveProcess (pprc);

            if (!CeSafeCopyMemory (pAddr, pBuf, cbSize)) {
                // try copy-on-write only if pprc is a debuggee controlled by the calling process
                dwErr = ((pprc == g_pprcNK)
                          || (pActvProc == g_pprcNK)
                          || !(pprc->pDbgrThrd)
                          || (pprc->pDbgrThrd->pprcOwner != pth->pprcOwner))
                                ? ERROR_INVALID_PARAMETER
                                : VMCopyOnWrite (pprc, pAddr, pBuf, cbSize);
            }

            SwitchActiveProcess (pprcOldActv);
            SwitchVM (pprcOldVM);
        }

        if ((_localCopy != pLocalCopy) && pLocalCopy) {
            VMFreeAndRelease (g_pprcNK, pLocalCopy, cbSize);
        }
    }

    pth->tlsPtr[TLSSLOT_KERNEL] = dwSaveKrn;

    return dwErr;
}


typedef struct {
    DWORD dwMask;
    DWORD dwNewFlags;
    DWORD dw1stEntry;
} VSAStruct, *PVSAStruct;

//-----------------------------------------------------------------------------------------------------------------
//
// ChangeAttrib: 2nd level PT enumeration funciton. change paget table entry attributes
//               based on mask and newflags
//
static BOOL ChangeAttrib (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVSAStruct pvsa = (PVSAStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    // 1st call - update dwIstEntry

    if (!pvsa->dw1stEntry) {
        pvsa->dw1stEntry = dwEntry;
    }
    *pdwEntry = (dwEntry & ~pvsa->dwMask) | (pvsa->dwMask & pvsa->dwNewFlags);
    return TRUE;
}



//
// VMSetAttributes - change the attributes of a range of VM
//
BOOL VMSetAttributes (PPROCESS pprc, LPVOID lpvAddress, DWORD cbSize, DWORD dwNewFlags, DWORD dwMask, LPDWORD lpdwOldFlags)
{
    DWORD dwErr = 0;
    VSAStruct vsa = { dwMask, dwNewFlags, 0 };

    DEBUGMSG (ZONE_VIRTMEM, (L"VMSetAttributes: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                        pprc, lpvAddress, cbSize, dwNewFlags, dwMask, lpdwOldFlags));

    DEBUGCHK (pActvProc == g_pprcNK);

    // validate parameter
    if ((int) cbSize <= 0) {                                                  // size is too big
        dwErr = ERROR_INVALID_PARAMETER;

    } else {

        DWORD dwAddr = (DWORD) lpvAddress & ~VM_PAGE_OFST_MASK;             // page align the address
        DWORD cPages = PAGECOUNT (cbSize + ((DWORD) lpvAddress - dwAddr));  // total pages
        DWORD cpNeeded = 0;                                                 // # of uncommited pages

        DEBUGMSG (dwMask & ~PG_CHANGEABLE,
            (L"WARNING: VMSetAttributes changing bits that is not changeable, dwMask = %8.8lx\r\n", dwMask));

        if (dwAddr >= VM_SHARED_HEAP_BASE) {
            pprc = g_pprcNK;
        }

        if (LockVM (pprc)) {

            if (!VMScan (pprc, dwAddr, cPages, &cpNeeded, 0)            // pages in the same reserved region?
                || cpNeeded) {                                          // all page committed?
                dwErr = ERROR_INVALID_PARAMETER;
            } else {
                VERIFY (Enumerate2ndPT (pprc->ppdir, dwAddr, cPages, 0, ChangeAttrib, &vsa));
            }

            UnlockVM (pprc);
        } else {
            dwErr = ERROR_INVALID_PARAMETER;
        }

    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    } else if (lpdwOldFlags) {
        VERIFY (CeSafeCopyMemory (lpdwOldFlags, &vsa.dw1stEntry, sizeof (DWORD)));
    }

    return !dwErr;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMAllocCopy: (internal only) reserve VM and VirtualCopy from source proceess/address.
//              Destination process is always the process whose VM is in use (pVMProc)
//
LPVOID
VMAllocCopy (
    PPROCESS pprcSrc,               // source process
    PPROCESS pprcDst,               // destination process
    LPVOID   pAddr,                 // address to be copy from
    DWORD    cbSize,                // size of the ptr
    DWORD    fProtect,              // protection,
    PALIAS   pAlias                 // ptr to alias structure
    )
{
    DWORD dwOfst = (DWORD) pAddr & VM_PAGE_OFST_MASK;
    DWORD dwAddr = (DWORD) pAddr & ~VM_PAGE_OFST_MASK;
    DWORD dwBlkOfst = ((DWORD)pAddr & VM_BLOCK_OFST_MASK);
    LPBYTE pDst = VMReserve (pprcDst, cbSize + dwBlkOfst, 0, 0);
    DWORD dwErr = 0;

    if (!pDst) {
        dwErr  = ERROR_NOT_ENOUGH_MEMORY;

    } else {
        DWORD cPages;
        pDst   += dwBlkOfst - dwOfst;       // page align destination
        cbSize += dwOfst;                   // size, starting from paged aligned address
        cPages = PAGECOUNT (cbSize);

        if ((PAGE_PHYSICAL & fProtect) || IsKernelVa (pAddr)) {
            // NOTE: call to PFNFrom256 must use original address, for the physical address is already shifted
            //       right by 8, masking bottom 12 bits can result in wrong physical address.
            DWORD dwPfn = (PAGE_PHYSICAL & fProtect)? PFNfrom256 ((DWORD) pAddr) : GetPFN ((LPVOID) dwAddr);
            DEBUGCHK (INVALID_PHYSICAL_ADDRESS != dwPfn);

            VERIFY (VMCopyPhysical (pprcDst, (DWORD) pDst, dwPfn, cPages, PageParamFormProtect (fProtect&~PAGE_PHYSICAL, (DWORD) pDst)));

        } else {
            DWORD _pPFNs[64];   // local buffer up to 64 pages
            LPDWORD pPFNs = _pPFNs;

            if ((cPages > 64) && !(pPFNs = NKmalloc (cPages * sizeof (DWORD)))) {
                dwErr = ERROR_NOT_ENOUGH_MEMORY;

            } else if (!(dwErr = DoVMLockPages (pprcSrc, dwAddr, cPages, pPFNs, (PAGE_READWRITE & fProtect)? VM_LF_WRITE : VM_LF_READ))) {

                if (!VMSetPages (pprcDst, (DWORD) pDst, pPFNs, cPages, fProtect|PAGE_PHYSICAL)) {
                    dwErr = ERROR_NOT_ENOUGH_MEMORY;
                }
#ifdef ARM
                // For virtually-tagged cache AllocateAlias adds PAGE_NOCACHE
                // to fProtect, so the dest buffer will be uncached.  But the
                // source buffer still needs to be uncached.
                else if (pAlias && !VMAlias (pAlias, pprcSrc, dwAddr, cbSize, fProtect)) {
                    dwErr = ERROR_NOT_ENOUGH_MEMORY;
                }
#endif
                VERIFY (!DoVMUnlockPages (pprcSrc, dwAddr, cPages));
            }

            if (_pPFNs != pPFNs) {
                NKfree (pPFNs);
            }
        }

        if (dwErr) {
            VERIFY (VMFreeAndRelease (pprcDst, pDst, cbSize));
        } else {
            pDst += dwOfst;
        }
    }

    if (dwErr) {
        NKSetLastError (dwErr);
    }
    return dwErr? NULL : pDst;

}


//-----------------------------------------------------------------------------------------------------------------
//
// VMInit: Initialize per-process VM (called at process creation)
//
BOOL VMInit (PPROCESS pprc)
{
    BOOL fRet = FALSE;
    PPAGETABLE pptbl = AllocatePTBL ();
    if (pptbl) {
        if ((pprc == g_pprcNK) || (pprc->ppdir = AllocatePD ())) {
            int idx;
            DWORD dwEntry = EntryFromReserveType (0);
            DEBUGCHK (pprc->ppdir);

            // 1st 64K reseved no access, except UserKData page
            for (idx = 0; idx < VM_PAGES_PER_BLOCK; idx ++) {
                pptbl->pte[idx] = dwEntry;
            }
            // page 5 access to KPAGE
            pptbl->pte[VM_KPAGE_IDX] = KPAGE_PTE;                  // setup UseKPage access

            // setup page directory
            SetupPDEntry (pprc->ppdir, 0, pptbl, (pprc == g_pprcNK));

            // initialize globals on 1st initialization
            if (pprc == g_pprcNK) {

                DWORD dwKDllFirst = (pTOC->dllfirst << 16);
                DWORD cbKDlls     = (pTOC->dlllast  << 16) - dwKDllFirst;

                // we're using VM of NK
                pVMProc = g_pprcNK;

                // reserve 1st 64K of kernel VM, kernel XIP DLL area, and shared heap for protection
                VERIFY (VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, 0));
                VERIFY (VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, VM_SHARED_HEAP_BASE));
                VERIFY (VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, VM_KDLL_BASE));

                //
                // Reserve VM for XIP kernel DLLs.
                //
                DEBUGMSG (1, (L"Reserve VM for kernel XIP DLls, first = %8.8lx, last = %8.8lx\r\n",
                                dwKDllFirst, dwKDllFirst + cbKDlls));
                DEBUGCHK (dwKDllFirst == g_vaKDllFree);

                VERIFY (VMReserve (g_pprcNK, cbKDlls, MEM_IMAGE, VM_KDLL_BASE));
                pprc->bASID = (BYTE) MDGetKernelASID ();

            } else {
                // reserved 64K VM at the beginning of VM_DLL_BASE, as region barrier
                VERIFY (VMAlloc (pprc, (LPVOID) VM_DLL_BASE, VM_BLOCK_SIZE, MEM_RESERVE, PAGE_NOACCESS));
                pprc->bASID = (BYTE) MDAllocateASID ();
            }


            fRet = TRUE;
        } else {
            // out of memory when allocating Page directory
            FreePhysPage (GetPFN (pptbl));
        }
    }

    DEBUGMSG (!fRet, (L"VMInit Failed, pptbl = %8.8lx, pprc->ppdir = %8.8lx\r\n", pptbl, pprc->ppdir));
    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// FreeAllPagesInPTBL: Decommit all pages in a page table
//
static void FreeAllPagesInPTBL (PPAGETABLE pptbl)
{
    int idx;
    DWORD dwEntry;

    for (idx = 0; idx < VM_NUM_PT_ENTRIES; idx ++) {
        dwEntry = pptbl->pte[idx];
        if (IsPageCommitted (dwEntry)) {
            FreePhysPage (PFNfromEntry(dwEntry));
        }
    }
 }

//-----------------------------------------------------------------------------------------------------------------
//
// VMDelete: Delete per-process VM (called when a process is fully exited)
//
void VMDelete (PPROCESS pprc)
{
    PPAGEDIRECTORY ppdir = LockVM (pprc);
    if (ppdir) {

        PPAGETABLE pptbl = Entry2PTBL (ppdir->pte[0]);
        // we know that VMInit will create the 0th pagetable 1st. If it doesn't exit,
        // there there is no 2nd-level page table.

        pprc->ppdir = NULL;

        UnlockVM (pprc);

        // debug build softlog, useful to find cache issue
        SoftLog (0x11111111, pprc->dwId);
        if (pptbl) {
            int idx = 0;
            DWORD dwEntry;
            // clear the User KPage entry, so we don't free it
            pptbl->pte[VM_KPAGE_IDX] = 0;

            OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD);
            for (idx = 0; idx < VM_LAST_USER_PDIDX; idx = NextPDEntry (idx)) {
                dwEntry = ppdir->pte[idx];
                if (pptbl = Entry2PTBL (dwEntry)) {
                    FreeAllPagesInPTBL (pptbl);
                    FreePhysPage (GetPFN (pptbl));
                }
            }

        }
        // free page directory
        FreePD (ppdir);

        // asid for the process is no long used
        MDFreeASID (pprc->bASID);

        // debug build softlog, useful to find cache issue
        SoftLog (0x11111112, pprc->dwId);
    }
 }

//-----------------------------------------------------------------------------------------------------------------
//
// VMCommit: commit memory (internal only)
//
LPVOID
VMCommit (
    PPROCESS pprc,          // process
    LPVOID lpvaddr,         // starting address
    DWORD  cbSize,          // size, in byte, of the allocation
    DWORD  fProtect,
    DWORD  dwPageType       // VM_PT_XXX
    )
{
    DWORD dwErr = 0;
#ifdef DEBUG
    LPVOID pAddr = lpvaddr;
#endif
    // internal call, arguements must be 'perfect' aligned
    DEBUGCHK (!((DWORD) lpvaddr & VM_PAGE_OFST_MASK));

    if (!(lpvaddr = DoVMAlloc (pprc,
                    (DWORD)lpvaddr,
                    PAGECOUNT (cbSize),
                    MEM_COMMIT,
                    fProtect,
                    dwPageType,
                    &dwErr))) {
        KSetLastError (pCurThread, dwErr);
    }

    DEBUGMSG (!lpvaddr, (L"VMCommit failed to commit addr 0x%8.8lx, of size 0x%8.8lx\r\n", pAddr, cbSize));
    return lpvaddr;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMAlloc: main function to allocate VM.
//
LPVOID
VMAlloc (
    PPROCESS pprc,          // process
    LPVOID lpvaddr,         // starting address
    DWORD  cbSize,          // size, in byte, of the allocation
    DWORD  fAllocType,      // allocation type
    DWORD  fProtect         // protection
    )
{
    DWORD    dwAddr = (DWORD) lpvaddr;
    DWORD    dwEnd  = dwAddr + cbSize;
    DWORD    dwErr  = 0;
    LPVOID   lpRet  = NULL;

    // verify arguments
    if (!IsValidAllocType (dwAddr, fAllocType)              // valid fAllocType?
        || ((int) cbSize <= 0)                              // valid size?
        || !IsValidProtect (fProtect)) {                    // valid fProtect?
        dwErr = ERROR_INVALID_PARAMETER;
        DEBUGMSG (ZONE_VIRTMEM, (L"VMAlloc failed, invalid parameter %8.8lx, %8.8lx %8.8lx %8.8lx\r\n",
            lpvaddr, cbSize, fAllocType, fProtect));
    } else {
        DWORD cPages;           // Number of pages

        // page align start address
        dwAddr &= ~VM_PAGE_OFST_MASK;
        cPages = PAGECOUNT (dwEnd - dwAddr);
        lpRet = DoVMAlloc (pprc, dwAddr, cPages, fAllocType, fProtect, PM_PT_ZEROED, &dwErr);

    }

    KSetLastError (pCurThread, dwErr);

    return lpRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMAllocPhys: allocate contiguous physical memory.
//
LPVOID VMAllocPhys (PPROCESS pprc, DWORD cbSize, DWORD fProtect, DWORD dwAlignMask, DWORD dwFlags, PULONG pPhysAddr)
{
    LPVOID pAddr = NULL;
    DWORD  dwPfn = INVALID_PHYSICAL_ADDRESS;
    DWORD  dwErr = ERROR_INVALID_PARAMETER;

    DEBUGMSG (ZONE_VIRTMEM, (L"VMAllocPhys: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                pprc->dwId, cbSize, dwAlignMask, dwFlags, pPhysAddr));

    // validate parameters
    if ((pActvProc == g_pprcNK)     // kernel only API
        && ((int)cbSize > 0)        // valid size
        && IsValidProtect(fProtect) // valid protection
        && pPhysAddr                // valid address to hold returned physical address
        && !dwFlags) {              // dwflags must be 0

        DWORD cPages = PAGECOUNT (cbSize);

        if (!(pAddr = VMReserve (pprc, cbSize, 0, 0))
            || ((dwPfn = GetContiguousPages (cPages, dwAlignMask, dwFlags)) == INVALID_PHYSICAL_ADDRESS)) {
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
            DEBUGMSG (ZONE_VIRTMEM, (L"VMAllocPhys: Couldn't get %d contiguous pages\r\n", cPages));

        } else {

            DEBUGMSG (ZONE_VIRTMEM, (L"VMAllocPhys: Got %d contiguous pages @ %8.8lx\r\n", cPages, dwPfn));
            dwErr = VMCopyPhysical (pprc, (DWORD) pAddr, dwPfn, cPages, PageParamFormProtect (fProtect|PAGE_NOCACHE, (DWORD) pAddr))
                ? 0
                : ERROR_NOT_ENOUGH_MEMORY;
        }

        if (dwErr) {
            // free up VM reserved
            if (pAddr) {
                VMRelease (pprc, (DWORD) pAddr, cPages);
            }

            // free up Physical Memory reserved
            if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
                OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_WRITEBACK);
                while (cPages --) {
                    FreePhysPage (dwPfn);
                    dwPfn = NextPFN (dwPfn);
                }
            }
        } else {

            __try {
                // need to discard L2 cache for we'll be accessing it uncached.
                OEMCacheRangeFlush (pAddr, cPages << VM_PAGE_SHIFT, CACHE_SYNC_L2_DISCARD);
                // we'll be zeroing the pages uncached. If this proves to be a perf issue, we can
                //    change this to zero pages cached, and flush cache.
                memset (pAddr, 0, cPages << VM_PAGE_SHIFT);
                *pPhysAddr = PFN2PA (dwPfn);
                if (!VMAddAllocToList (pprc, pAddr, cbSize, NULL)) {
                    dwErr = ERROR_NOT_ENOUGH_MEMORY;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                dwErr = ERROR_INVALID_PARAMETER;
            }
            if (dwErr) {
                // free up VM reserved, physical will be freed automatically.
                VMRelease (pprc, (DWORD) pAddr, cPages);
            }

        }

    }

    KSetLastError (pCurThread, dwErr);

    DEBUGMSG (ZONE_VIRTMEM, (L"VMAllocPhys: returns %8.8lx, (dwErr = %8.8lx)\r\n",
                    pAddr, dwErr));

    return dwErr? NULL : pAddr;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMReserve: (internal only) reserve VM in DLL/SharedHeap/ObjectStore
//
LPVOID
VMReserve (
    PPROCESS pprc,                  // which process
    DWORD   cbSize,                 // size of the reservation
    DWORD   fMemType,               // memory type (0, MEM_IMAAGE, MEM_MAPPED, MEM_AUTO_COMMIT)
    DWORD   dwSearchBase            // search base
                                    //      0 == same as VMAlloc (pprc, NULL, cbSize, MEM_RESERVE|fMemType, PAGE_NOACCESS)
                                    //      VM_DLL_BASE - reserve VM for DLL loading
                                    //      VM_SHARED_HEAP_BASE - reserve VM for shared heap
                                    //      VM_STATIC_MAPPING_BASE - reserve VM for static mapping
) {
    LPVOID lpRet = NULL;
    DWORD  cPages = PAGECOUNT (cbSize);

    if (LockVM (pprc)) {
        lpRet = (LPVOID) DoVMReserve (pprc, 0, cPages, dwSearchBase, fMemType);
        UnlockVM (pprc);
        CELOG_VirtualAlloc(pprc, (DWORD)lpRet, 0, cPages, fMemType, 0);
    }

    return lpRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMCopy: main function to VirtualCopy VM, kernel only, not exposed to user mode apps
//
BOOL
VMCopy (
    PPROCESS pprcDst,       // the destination process
    DWORD dwDestAddr,       // destination address
    PPROCESS pprcSrc,       // the source process, NULL if PAGE_PHYSICAL or same as destination
    DWORD dwSrcAddr,        // source address
    DWORD cbSize,           // size, in bytes
    DWORD fProtect          // protection
    )
{
    DWORD    dwErr = ERROR_INVALID_PARAMETER;
    BOOL     fPhys = (fProtect & PAGE_PHYSICAL);
    DWORD    dwOfstSrc = (fPhys? (dwSrcAddr << 8) : dwSrcAddr) & VM_PAGE_OFST_MASK;
    DWORD    dwOfstDest = dwDestAddr & VM_PAGE_OFST_MASK;
    BOOL     fRet = FALSE;

    fProtect &= ~PAGE_PHYSICAL;

    // verify arguments
    if (((pprcDst != g_pprcNK) && ((int) dwDestAddr < VM_USER_BASE))    // valid address?
        || ((int) cbSize <= 0)                                          // valid size?
        || (dwOfstSrc != dwOfstDest)                                    // not the same page offset
        || !IsValidProtect (fProtect)) {                                // valid fProtect?
        dwErr = ERROR_INVALID_PARAMETER;

    } else {

        // do the copy
        DWORD dwPgProt = PageParamFormProtect (fProtect, dwDestAddr);
        DWORD cPages = PAGECOUNT (cbSize + dwOfstDest); // Number of pages
        DWORD dwPfn = INVALID_PHYSICAL_ADDRESS;

        dwDestAddr -= dwOfstDest;

        if (!pprcSrc)
            pprcSrc = pprcDst;

        if (fPhys) {
            dwPfn = PFNfrom256 (dwSrcAddr);
        } else if (IsKernelVa ((LPVOID) dwSrcAddr)) {
            // kernel address, use physical version of the copy function as they're section mapped
            dwPfn = GetPFN ((LPVOID) dwSrcAddr);
        }

        fRet = (INVALID_PHYSICAL_ADDRESS != dwPfn)
            ? VMCopyPhysical (pprcDst, dwDestAddr, dwPfn, cPages, dwPgProt)
            : VMCopyVirtual (pprcDst, dwDestAddr, pprcSrc, dwSrcAddr - dwOfstSrc, cPages, dwPgProt, &dwErr);

        if (fRet) {
            CELOG_VirtualCopy(pprcDst, dwDestAddr, pprcSrc, dwSrcAddr, cPages, dwPfn, fProtect);
        }
    }

    if (!fRet) {
        DEBUGMSG (1, (L"VMCopy Failed, dwErr = %8.8lx\r\n", dwErr));
        KSetLastError (pCurThread, dwErr);
    }

    return fRet;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMFastCopy: (internal only) duplicate VM mapping from one to the other.
// NOTE: (1) This is a fast function, no scanning, validation is performed.
//       (2) Source and destination must both be Virtual addresses and must be page aligned.
//       (3) source and destination must have the same block offset, unless source is static-mapped kernel
//           address.
//       (4) Caller must be verifying all parameters before calling this.
//
BOOL VMFastCopy (
    PPROCESS pprcDst,       // the destination process
    DWORD dwDstAddr,        // destination address
    PPROCESS pprcSrc,       // the source process, NULL if PAGE_PHYSICAL or same as destination
    DWORD dwSrcAddr,        // source address
    DWORD cbSize,           // # of pages
    DWORD fProtect
    )
{
    BOOL fRet;
    DWORD dwPgProt = PageParamFormProtect (fProtect, dwDstAddr);
    DWORD cPages   = PAGECOUNT (cbSize);

    DEBUGCHK (!(dwDstAddr & VM_PAGE_OFST_MASK));
    DEBUGCHK (!(dwSrcAddr & VM_PAGE_OFST_MASK));
    DEBUGCHK (cPages);
    DEBUGCHK (IsKernelVa ((LPCVOID) dwSrcAddr)
              || ((dwDstAddr & VM_BLOCK_OFST_MASK) == (dwSrcAddr & VM_BLOCK_OFST_MASK)));

    if (IsKernelVa ((LPCVOID) dwSrcAddr)) {
        fRet = VMCopyPhysical (pprcDst, dwDstAddr, GetPFN ((LPCVOID) dwSrcAddr), cPages, dwPgProt);

    } else if (fRet = Lock2VM (pprcDst, pprcSrc)) {
        PPAGEDIRECTORY ppdir = pprcSrc->ppdir;
        DWORD         idxdir = VA2PDIDX(dwSrcAddr);
        PPAGETABLE     pptbl = Entry2PTBL (ppdir->pte[idxdir]);
        VCVirtStruct    vcvs = { ppdir, pptbl, idxdir, VA2PT2ND(dwSrcAddr), dwPgProt, 0 };

        LockPhysMem ();
        fRet = Enumerate2ndPT (pprcDst->ppdir, dwDstAddr, cPages, 0, VCVirtPages, &vcvs);
        UnlockPhysMem ();

        UnlockVM (pprcSrc);
        UnlockVM (pprcDst);
    }

    return fRet;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMFreeAndRelease - decommit and release VM
//
BOOL
VMFreeAndRelease (PPROCESS pprc, LPVOID lpvAddr, DWORD cbSize)
{
    DWORD dwOfst = (DWORD)lpvAddr & VM_BLOCK_OFST_MASK;
    return !VMRelease (pprc, (DWORD)lpvAddr-dwOfst, PAGECOUNT (cbSize+dwOfst));
}

typedef struct _QueryEnumStruct {
    DWORD dwMatch;
    DWORD cPages;
} QueryEnumStruct, *PQueryEnumStruct;

//-----------------------------------------------------------------------------------------------------------------
//
// CountSamePages: 2nd level PT enumeration funciton. check if all entries are committed or all of them are uncommited.
//
static BOOL CountSamePages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PQueryEnumStruct pqe = (PQueryEnumStruct) pEnumData;

    if (IsSameEntryType (*pdwEntry, pqe->dwMatch)) {
        pqe->cPages ++;
        return TRUE;
    }

    // not match, stop enumeration
    return FALSE;
}



//-----------------------------------------------------------------------------------------------------------------
//
// VMQuery: function to Query VM
//
DWORD
VMQuery (
    PPROCESS pprc,                      // process
    LPVOID lpvaddr,                     // address to query
    PMEMORY_BASIC_INFORMATION  pmi,     // structure to fill
    DWORD  dwLength                     // size of the buffer allocate for pmi
)
{
    DWORD    dwAddr = (DWORD) lpvaddr & ~VM_PAGE_OFST_MASK;
    DWORD    cbRet  = sizeof (MEMORY_BASIC_INFORMATION);
    DWORD    dwErr  = 0;

    //
    // NOTE: pointer validation (pmi) must have been done before calling this function
    //

    // verify arguments
    if ((int) dwLength < sizeof (MEMORY_BASIC_INFORMATION)) {   // valid size?
        dwErr = ERROR_BAD_LENGTH;

    } else if (dwAddr < VM_USER_BASE) {                         // valid address?
        dwErr = ERROR_INVALID_PARAMETER;

    } else {

        DWORD idxdir = VA2PDIDX (dwAddr);
        DWORD idx2nd = VA2PT2ND (dwAddr);
        PPAGEDIRECTORY ppdir = LockVM (pprc);
        PPAGETABLE pptbl;
        DWORD cPages;

        if (ppdir) {

            pptbl = Entry2PTBL (ppdir->pte[idxdir]);
            if (!pptbl || (VM_FREE_PAGE == pptbl->pte[idx2nd])) {

                // dwAddr is in a free region

                // count the # of free pages from next block
                cPages = CountFreePages (ppdir, (dwAddr+VM_BLOCK_SIZE) & ~VM_BLOCK_OFST_MASK, VM_MAX_PAGES);

                // add the # of free pages in this block
                cPages += VM_PAGES_PER_BLOCK - (idx2nd % VM_PAGES_PER_BLOCK);

                // update the structure
                pmi->AllocationBase = NULL;
                pmi->Protect        = PAGE_NOACCESS;
                pmi->State          = MEM_FREE;
                pmi->Type           = MEM_PRIVATE;

            } else {

                // dwAddr is in a reserved region
                DWORD dwEntry = pptbl->pte[idx2nd];
                QueryEnumStruct qe = { dwEntry, 0 };

                // enumerate PT to find all matched entries
                Enumerate2ndPT (ppdir, dwAddr, VM_MAX_PAGES, 0, CountSamePages, &qe);

                // update cPages
                cPages = qe.cPages;

                // find the reservation base
                pmi->AllocationBase = (LPVOID) FindReserveBase (pprc, dwAddr & ~VM_BLOCK_OFST_MASK);

                // update protect/state/type
                if (IsPageCommitted (dwEntry)) {
                    pmi->Protect    = ProtectFromEntry (dwEntry);
                    pmi->State      = MEM_COMMIT;
                    pmi->Type       = MemTypeFromAddr (pprc, dwAddr);
                } else {
                    pmi->Protect    = PAGE_NOACCESS;
                    pmi->State      = MEM_RESERVE;
                    pmi->Type       = MemTypeFromReservation (dwEntry);
                }
            }

            UnlockVM (pprc);

            // common part
            pmi->AllocationProtect  = PAGE_NOACCESS;     // allocation protect always PAGE_NOACCESS
            pmi->BaseAddress        = (LPVOID) dwAddr;
            pmi->RegionSize         = cPages << VM_PAGE_SHIFT;

        }
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
        cbRet = 0;
    }

    return cbRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMLockPages: function to lock VM, kernel only, not exposed to user mode apps
//
BOOL
VMLockPages (
    PPROCESS pprc,              // process
    LPVOID lpvaddr,             // address to query
    DWORD  cbSize,              // size to lock
    LPDWORD pPFNs,              // the array to retrieve PFN
    DWORD   fOptions            // options: see LOCKFLAG_*
)
{
    DWORD dwAddr = (DWORD) lpvaddr & ~VM_PAGE_OFST_MASK;
    DWORD dwErr = 0;

    DEBUGMSG (pCurThread && !pCurThread->bDbgCnt && ZONE_VIRTMEM, (L"VMLockPages (%8.8lx, %8.8lx, %8.8lx, %8.8lx, %8.8lx), pprc->csVM.OwnerThread = %8.8lx\r\n",
            pprc, lpvaddr, cbSize, pPFNs, fOptions, pprc->csVM.OwnerThread));
//    if (((pprc != g_pprcNK) && ((int) dwAddr < VM_USER_BASE))       // valid address?
    if ((dwAddr < VM_USER_BASE)             // valid address?
        || ((int) cbSize <= 0)              // valid size?
        || !IsValidLockOpt (fOptions)) {    // valid options?
        dwErr = ERROR_INVALID_PARAMETER;

    } else {
        DWORD cPages = PAGECOUNT (cbSize + ((DWORD) lpvaddr - dwAddr));

        // special treatment for kernel addresses
        if (IsKernelVa (lpvaddr)) {
            // kernel address, section mapped
            if (!IsKernelVa ((LPBYTE) lpvaddr + cbSize)) {
                dwErr = ERROR_INVALID_PARAMETER;
            } else if (pPFNs) {
                DWORD dwPFN;
                for (dwPFN = GetPFN ((LPVOID) dwAddr); cPages --; pPFNs ++, dwPFN = NextPFN (dwPFN)) {
                    *pPFNs = dwPFN;
                }
            }
        } else {
            dwErr = DoVMLockPages (pprc, dwAddr, cPages, pPFNs, fOptions);
        }
    }


    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    return !dwErr;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMUnlockPages: function to unlock VM, kernel only, not exposed to user mode apps
//
BOOL
VMUnlockPages (
    PPROCESS pprc,              // process
    LPVOID lpvaddr,             // address to query
    DWORD  cbSize               // size to lock
)
{
    DWORD dwAddr = (DWORD) lpvaddr & ~VM_PAGE_OFST_MASK;
    DWORD dwErr = 0;

    DEBUGMSG (pCurThread && !pCurThread->bDbgCnt && ZONE_VIRTMEM, (L"VMUnlockPages (%8.8lx, %8.8lx, %8.8lx)\r\n",
        pprc, lpvaddr, cbSize));

//    if (((pprc != g_pprcNK) && ((int) dwAddr < VM_USER_BASE))       // valid address?
    if ((dwAddr < VM_USER_BASE)             // valid address?
        || ((int) cbSize <= 0)) {           // valid size?
        dwErr = ERROR_INVALID_PARAMETER;

    } else if (!IsKernelVa (lpvaddr)) {
        DWORD cPages = PAGECOUNT (cbSize + ((DWORD) lpvaddr - dwAddr));
        dwErr = DoVMUnlockPages (pprc, dwAddr, cPages);

    }


    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    return !dwErr;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMProtect: main function to change VM protection.
//
BOOL
VMProtect (
    PPROCESS pprc,              // process
    LPVOID lpvaddr,             // starting address
    DWORD  cbSize,              // size, in byte, of the allocation
    DWORD  fNewProtect,         // new protection
    LPDWORD pfOldProtect        // old protect value
    )
{
    DWORD dwAddr = (DWORD) lpvaddr & ~VM_PAGE_OFST_MASK;
    DWORD dwErr = 0;

    DEBUGCHK ((pprc == g_pprcNK) || ((int) dwAddr >= VM_USER_BASE));
    DEBUGCHK ((int) cbSize > 0);

    if (!IsValidProtect (fNewProtect)) {      // valid protection?
        dwErr = ERROR_INVALID_PARAMETER;

    } else {
        DWORD cPages = PAGECOUNT (cbSize + ((DWORD) lpvaddr - dwAddr));

        dwErr = DoVMProtect (pprc, dwAddr, cPages, fNewProtect, pfOldProtect);
    }


    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    return !dwErr;
}

#ifdef ARM

void VMAddUncachedAllocation (PPROCESS pprc, DWORD dwAddr, PVALIST pUncached)
{
    if (LockVM (pprc)) {

        PVALIST pVaItem = NULL;

        if (!FindVAItem (&pprc->pUncachedList, dwAddr, FALSE)) {

            // find the reservation of the address to be uncached
            pVaItem = FindVAItem (&pprc->pVaList, dwAddr, FALSE);

            if (!pVaItem) {
                // Someone VirtualFree'd a Virtual Allocation *before* VirtualAlloc returned.
                // Most likely freeing a dangling pointer, or malicious attack. Don't need to
                // keep track of it.
                DEBUGCHK (0);
            } else {
                pUncached->cbVaSize = pVaItem->cbVaSize;
                pUncached->pVaBase  = pVaItem->pVaBase;
                pUncached->pPTE     = NULL;
                pUncached->pNext    = pprc->pUncachedList;
                pprc->pUncachedList = pUncached;
            }
        }
        UnlockVM (pprc);

        if (!pVaItem) {
            // allocation already exists in uncached list, or the debugchk above. Free pUncached
            FreeMem (pUncached, HEAP_VALIST);
        }
    }
}


typedef struct {
    PPROCESS pprc;
    DWORD    dwAddr;
    PVALIST  pUncachedList;
} EnumAliasStruct, *PEnumAliasStruct;

void KC_UncacheOnePage (LPDWORD pdwEntry, LPVOID pAddr)
{
    KCALLPROFON (0);
    *pdwEntry &= ~PG_CACHE_MASK;
    OEMCacheRangeFlush (pAddr, VM_PAGE_SIZE, CACHE_SYNC_DISCARD | CACHE_SYNC_FLUSH_D_TLB);
    KCALLPROFOFF (0);
}

PFREEINFO GetRegionFromPFN (DWORD dwPfn);
BOOL SetMemoryAttributes (LPVOID pVirtualAddr, LPVOID pShiftedPhysAddr, DWORD cbSize, DWORD dwAttributes);

static BOOL UncachePage (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PEnumAliasStruct peas = (PEnumAliasStruct) pEnumData;
    DWORD dwEntryCacheMode = *pdwEntry & PG_CACHE_MASK;

    DEBUGCHK (IsPageCommitted (*pdwEntry));

    if (dwEntryCacheMode    // cache enabled
        && GetRegionFromPFN (PFNfromEntry (*pdwEntry))  // RAM address
    ) {
        PVALIST pUncachedItem = peas->pUncachedList;

        do {
            // if peas->dwAddr is out of current UncachedItem's cover range,
            // search or create a new item for tracking.
            if (!pUncachedItem || (peas->dwAddr - (DWORD) pUncachedItem->pVaBase >= pUncachedItem->cbVaSize)) {
                // try to find in process' Uncached List first
                pUncachedItem = FindVAItem (&peas->pprc->pUncachedList, peas->dwAddr, FALSE);

                // still not able to find one, create a new Uncached Item.
                if (!pUncachedItem)
                {
                    PVALIST pVaItem;
                    pUncachedItem = AllocMem (HEAP_VALIST);
                    DEBUGCHK (pUncachedItem);

                    if (!pUncachedItem) {
                        DEBUGMSG (1, (L"UncachePage: can't allocate uncached list (OOM)\r\n"));
                        return FALSE;
                    }
                    // find the reservation of the address to be uncached
                    pVaItem = FindVAItem (&peas->pprc->pVaList, peas->dwAddr, FALSE);

                    if (!pVaItem) {
                        // (1) In the between of VirtualFree, so the VaList is cleared but page not yet freed.
                        // (2) Source buffer is not in heap (e.g. stack)
                        // using block align address as base
                        pUncachedItem->cbVaSize = VM_BLOCK_SIZE;
                        pUncachedItem->pVaBase  = (PVOID)BLOCKALIGN_DOWN(peas->dwAddr);
                    } else {
                        pUncachedItem->cbVaSize = pVaItem->cbVaSize;
                        pUncachedItem->pVaBase  = pVaItem->pVaBase;
                    }
                    pUncachedItem->pPTE     = NKmalloc (PAGECOUNT(pUncachedItem->cbVaSize) * sizeof(PTENTRY));

                    if (!pUncachedItem->pPTE) {
                        FreeMem (pUncachedItem, HEAP_VALIST);
                        DEBUGMSG (1, (L"UncachePage: can't allocate backup PTE list (OOM)\r\n"));

                        return FALSE;
                    }

                    memset (pUncachedItem->pPTE, 0, PAGECOUNT(pUncachedItem->cbVaSize) * sizeof(PTENTRY));
                    pUncachedItem->pNext = peas->pUncachedList;
                    peas->pUncachedList = pUncachedItem;
                }
            }

            if (pUncachedItem->pPTE) {
                DWORD ixPage = PAGECOUNT (peas->dwAddr - (DWORD)pUncachedItem->pVaBase);
                if (!pUncachedItem->pPTE[ixPage]) {
                    pUncachedItem->pPTE[ixPage] = *pdwEntry;
                }
                else if (PFNfromEntry (pUncachedItem->pPTE[ixPage]) != PFNfromEntry (*pdwEntry)) {
                    DEBUGMSG (1, (L"UncachePage: remove stale UncachedItem PFN mismatch at %8.8lx:%8.8lx expect %8.8lx actual %8.8lx\r\n",
                                                peas->pprc->dwId, peas->dwAddr, PFNfromEntry(pUncachedItem->pPTE[ixPage]), PFNfromEntry (*pdwEntry)));

                    // remove stale Uncached Item (when backup PTE's PFN not equal to actual PFN)
                    VERIFY(FindVAItem (&peas->pprc->pUncachedList, (DWORD)pUncachedItem->pVaBase, TRUE));
                    peas->pUncachedList = peas->pprc->pUncachedList;
              NKfree (pUncachedItem->pPTE);
                    FreeMem (pUncachedItem, HEAP_VALIST);
                    pUncachedItem = NULL;
                }
            }
            else {
                DEBUGMSG (1, (L"UncachePage: %8.8lx:%8.8lx was committed as PAGE_NOCACHE (UncachedItem->pPTE == NULL) but actual cache mode is %8.8lx\r\n",
                                            peas->pprc->dwId, peas->dwAddr, *pdwEntry & PG_CACHE_MASK));
                
                DEBUGCHK (0);
            }
        }  while (!pUncachedItem);

        // we only need to flush cache if we're in current VM, or kernel address
        if ((peas->pprc == g_pprcNK) || (peas->pprc == pVMProc) || !pCurThread->pprcVM) {
            // we need to do flush+turning off cache in a KCall. Otherwise we'll get into trouble
            // if we got preempted and the page got brought into cache again.
            KCall ((PKFN) KC_UncacheOnePage, pdwEntry, peas->dwAddr);
        } else {
            // The VM is not active, there is no way there's anything of this address in cache.
            // just remove the cache bit
            *pdwEntry &= ~PG_CACHE_MASK;
        }

        // note: OEMSetMemoryAttributes can only set cache attribute for
        //       kernel memory and address belong to current active VM(pVMProc)
        if ((peas->dwAddr >= VM_SHARED_HEAP_BASE) || (peas->pprc == pVMProc)) {
            SetMemoryAttributes ((LPVOID)peas->dwAddr, (LPVOID)PA256FromPfn(*pdwEntry & PG_PHYS_ADDR_MASK), VM_PAGE_SIZE, PAGE_WRITECOMBINE);
        }

        DEBUGMSG (ZONE_VIRTMEM, (L"Page at %8.8lx:%8.8lx is uncached\r\n", peas->pprc->dwId, peas->dwAddr));
    }

    peas->dwAddr += VM_PAGE_SIZE;
    return TRUE;
}


BOOL
VMAlias (
    PALIAS   pAlias,
    PPROCESS pprc,
    DWORD    dwAddr,
    DWORD    cbSize,
    DWORD    fProtect)
{
    BOOL fRet;

    DEBUGCHK (!(dwAddr & VM_PAGE_OFST_MASK));
    DEBUGCHK ((int) cbSize > 0);
    DEBUGCHK (IsVirtualTaggedCache ());

    if (dwAddr >= VM_SHARED_HEAP_BASE) {
        pprc = g_pprcNK;
    }

    // once we turn views uncached, we can't turn cache back on (probably possible
    // with some very complicated scanning). We'll investigate the possibility in the
    // future if this turns out to be a perf killer.
    if (!(fRet = MAPUncacheViews (pprc, dwAddr, cbSize))) {

        // TBD - perf improvement
        // alias to code, don't bother turning off cache.
        //
        //if (!(fProtect & PAGE_READWRITE) && IsCodeAddr (pAddr, cbSize)) {
        //    return TRUE;
        //}

        PPAGEDIRECTORY ppdir = LockVM (pprc);

        if (ppdir) {
            fRet = IsVMAllocationAllowed (pprc);
            DEBUGCHK (fRet);    // VirtualAllocCopy to a process that is in final stage of exiting...

            if (fRet) {
                EnumAliasStruct eas = { pprc, dwAddr, pprc->pUncachedList };

                fRet = Enumerate2ndPT (ppdir, dwAddr, PAGECOUNT (cbSize), FALSE, UncachePage, &eas);

                // Update the uncached list.
                pprc->pUncachedList = eas.pUncachedList;
            }
            UnlockVM (pprc);

            // record the alias even failed, so VMUnalias can clean up the alias.
            pAlias->dwAliasBase = dwAddr;
            pAlias->cbSize      = cbSize;
            pAlias->dwProcessId = pprc->dwId;
        }
    }

    return fRet;
}

static BOOL TryCachePage (LPDWORD pdwEntry, LPVOID pEnumData)
{

    PEnumAliasStruct peas = (PEnumAliasStruct) pEnumData;

    if ((*pdwEntry & PG_CACHE_MASK) != PG_CACHE) {

        PVALIST pVaItem = peas->pUncachedList;

        // FindVAItem only if address is not in peas->pUncachedList. (multiple uncached regions)
        if (!pVaItem || (peas->dwAddr - (DWORD) pVaItem->pVaBase >= pVaItem->cbVaSize)) {
            // Is first node in peas->pUncachedList?
            BOOL fFirstNode = !pVaItem;

            // since first node does not necessary have to start from the beginning of  pVaItem->pVaBase
            // (this pVaItem can be created by another VirtualCopyEx allocation)
            // use peas->dwAddr to remove the item when not first node.
            pVaItem = FindVAItem (&peas->pprc->pUncachedList, peas->dwAddr, !fFirstNode);

            // if it is not first node and FindVAItem(..., TRUE) failed,
            // it should also fail in FindVAItem(..., FALSE)
            DEBUGCHK(fFirstNode || pVaItem || !FindVAItem (&peas->pprc->pUncachedList, peas->dwAddr, FALSE));

            RETAILMSG((!fFirstNode && !pVaItem && FindVAItem (&peas->pprc->pUncachedList, peas->dwAddr, FALSE)),
                (L"address %8.8lx is not in beginning of any item of UncachedList\r\n",
                peas->dwAddr, FindVAItem (&peas->pprc->pUncachedList, peas->dwAddr, FALSE)->pVaBase
                ));

            if (pVaItem) {
                VERIFY(!fFirstNode || (pVaItem == FindVAItem (&peas->pprc->pUncachedList, (DWORD)pVaItem->pVaBase, TRUE)));
                pVaItem->pNext = peas->pUncachedList;
                peas->pUncachedList = pVaItem;
            }
        }

        // can't find in Uncached List and current cache mode is not uncached when
        // (1) the original page is uncached and not listed in pprc->pUncachedList, but cache attribute changed after VMAlias.
        // (2) we are doing a clean up for a failed VMAlias, so only partial pages change cache mode.
        // (3) The source process VirtualFree the buffer while we're still using it, so the uncache entries are gone
        DEBUGMSG((!pVaItem && ((*pdwEntry & PG_CACHE_MASK) == PAGE_NOCACHE)),
            (L"page %8.8lx is not in UncachedList, page entry %8.8lx\r\n", peas->dwAddr, *pdwEntry)
            );

        // don't turn cache back if address is not in UncachedList
        // address is not recorded in UncachedList when
        // (1) the original page is uncached but not listed in pprc->pUncachedList (VMAllocPhys)
        // (2) we are doing a clean up for a failed VMAlias, so only partial pages change cache mode.
        // (3) pVaItem->pPTE == NULL, the original page is committed as PAGE_NOCACHE
        if (pVaItem
            && pVaItem->pPTE
            && (pVaItem->pPTE[PAGECOUNT (peas->dwAddr - (DWORD)pVaItem->pVaBase)] & PG_CACHE_MASK)
        ) {

            PFREEINFO        pfi;
            DWORD            dwPfn = PFNfromEntry (*pdwEntry);

            DEBUGCHK (OwnCS (&PhysCS));

            if (IsPageCommitted (*pdwEntry)
                && (pfi = GetRegionFromPFN (dwPfn))) {
                uint ix = (dwPfn - pfi->paStart) / PFN_INCR;
                uint ixPage = PAGECOUNT (peas->dwAddr - (DWORD)pVaItem->pVaBase);

                if (1 == pfi->pUseMap[ix]) {
                    // stop when PFN mismatched (pVaItem is a stale tracking item)
                    // (1) Uncached item is created after VirtualFree
                    // (2) source buffer is stack and the stack is destroyed
                    if (dwPfn != PFNfromEntry(pVaItem->pPTE[ixPage])) {
                        // clear all backup PTE, so it can be removed.
                        memset (pVaItem->pPTE, 0, PAGECOUNT (pVaItem->cbVaSize) * sizeof(PTENTRY));
                        DEBUGMSG (1, (L"detect stale UncachedItem PFN mismatch at %8.8lx:%8.8lx expect %8.8lx actual %8.8lx\r\n",
                            peas->pprc->dwId, peas->dwAddr, PFNfromEntry(pVaItem->pPTE[ixPage]), dwPfn));
                    }
                    else {
                        *pdwEntry &= ~PG_CACHE_MASK;
                        *pdwEntry |= pVaItem->pPTE[ixPage] & PG_CACHE_MASK;

                        // clear the cache mode when done
                        pVaItem->pPTE[ixPage] = 0;
                        DEBUGMSG (ZONE_VIRTMEM, (L"Turning cache back on for page at %8.8lx:%8.8lx\r\n", peas->pprc->dwId, peas->dwAddr));
                    }
                }
            }
        }

    }

    peas->dwAddr += VM_PAGE_SIZE;
    return TRUE;
}

BOOL VMUnalias (PALIAS pAlias)
{
    if (pAlias) {

        if (pAlias->dwAliasBase) {
            PHDATA phd = LockHandleData ((HANDLE) pAlias->dwProcessId, g_pprcNK);
            PPROCESS pprc = GetProcPtr (phd);
            PPAGEDIRECTORY ppdir;
            DEBUGCHK (!(pAlias->dwAliasBase & VM_PAGE_OFST_MASK));
            DEBUGCHK (pAlias->cbSize > 0);
            if (pprc && (ppdir = LockVM (pprc))) {

                EnumAliasStruct eas = {pprc, pAlias->dwAliasBase, FindVAItem (&pprc->pUncachedList, pAlias->dwAliasBase, TRUE)};

                // unlink the item.
                if (eas.pUncachedList)
                    eas.pUncachedList->pNext = NULL;

                LockPhysMem ();
                Enumerate2ndPT (ppdir, pAlias->dwAliasBase, PAGECOUNT (pAlias->cbSize), FALSE, TryCachePage, &eas);
                UnlockPhysMem ();

                if (eas.pUncachedList) {
                    PVALIST pUncachedList = eas.pUncachedList, pUncachedItem;

                    while (NULL != (pUncachedItem = pUncachedList))
                    {
                        DWORD cPage;
                        pUncachedList = pUncachedItem->pNext;

                        // free the item if all pages are restored.
                        cPage = PAGECOUNT (pUncachedItem->cbVaSize);
                        DEBUGCHK (cPage);
                        if (pUncachedItem->pPTE) {
                            // scan all page's backup PTE to see if all pages are restored.
                            while ((pUncachedItem->pPTE[cPage-1] == 0) && (--cPage)) ;
                        }

                        if (!cPage) {
                            DEBUGMSG (ZONE_VIRTMEM, (L"All pagess at %8.8lx, size %8.8lx turned cache back, remove tracking UncachedItem %8.8lx\r\n",
                                pUncachedItem->pVaBase, pUncachedItem->cbVaSize, pUncachedItem));
                            NKfree (pUncachedItem->pPTE);
                            FreeMem (pUncachedItem, HEAP_VALIST);
                        }
                        // only partial pages is restored, linked the page back the UncachedList
                        else {
                            pUncachedItem->pNext = pprc->pUncachedList;
                            pprc->pUncachedList = pUncachedItem;
                        }
                    }
                }
                UnlockVM (pprc);
            }
            UnlockHandleData (phd);
        }
        FreeMem (pAlias, HEAP_ALIAS);
    }
    return TRUE;
}

#endif

static DWORD PfnFromPDEntry (DWORD dwPDEntry)
{
#if defined (x86) || defined (ARM)
    return PFNfromEntry (dwPDEntry);
#else
    return dwPDEntry? GetPFN ((LPVOID) dwPDEntry) : 0;
#endif
}

static DoMappPD (PPAGETABLE pptbl, PPAGEDIRECTORY ppdir, DWORD idxPT,
                DWORD idxPDStart, DWORD idxPDEnd, LPBYTE pdbits)
{
    DWORD dwPfn;

#ifdef ARM
    // ARM - page table/directory access may need to be uncached.
    DWORD dwPgProt;
    if (g_pOemGlobal->dwTTBRCacheBits) {
        dwPgProt = PageParamFormProtect (PAGE_READONLY, VM_PD_DUP_ADDR);
    } else {
        dwPgProt = PageParamFormProtect (PAGE_READONLY|PAGE_NOCACHE, VM_PD_DUP_ADDR);
    }
#else
    DWORD dwPgProt = PageParamFormProtect (PAGE_READONLY, VM_PD_DUP_ADDR);
#endif
    LockPhysMem ();
    for ( ; idxPDStart < idxPDEnd; idxPT ++, idxPDStart = NextPDEntry (idxPDStart)) {
        if ((dwPfn = PfnFromPDEntry (ppdir->pte[idxPDStart]))
            && DupPhysPage (dwPfn)) {
            PREFAST_DEBUGCHK (idxPT < 512); // no more than 512 page tables
            pptbl->pte[idxPT] = MakeCommittedEntry (dwPfn, dwPgProt);
            pdbits[idxPT] = 1;
        }
    }
    UnlockPhysMem ();

}

static DWORD VMMapKernelPD (PPROCMEMINFO ppmi)
{
    DWORD dwRet = 0;
    PPROCESS pprc = pVMProc;

    // We don't need to lock kernel VM because pagetables of kernel never got freed.
    PPAGEDIRECTORY ppdir = LockVM (pprc);

    DEBUGCHK (ppdir);
    if (dwRet = DoVMReserve (pprc, VM_PD_DUP_ADDR, PAGECOUNT (VM_PD_DUP_SIZE), 0, 0)) {

        PPAGETABLE pptbl = Entry2PTBL (ppdir->pte[VA2PDIDX(dwRet)]);
        DEBUGCHK (pptbl);

        // Map from shared heap area (0x70000000 - 0x80000000)
        DoMappPD (pptbl, g_ppdirNK,
                        0,
                        VA2PDIDX (VM_SHARED_HEAP_BASE),
                        VA2PDIDX (VM_KMODE_BASE),
                        ppmi->pdbits);

        // Map from kernel XIP DLL area (0xC0000000 - 0xC8000000)
        DoMappPD (pptbl, g_ppdirNK,
                        (VM_KDLL_BASE - VM_SHARED_HEAP_BASE) >> 22,
                        VA2PDIDX (VM_KDLL_BASE),
                        VA2PDIDX (VM_OBJSTORE_BASE),
                        ppmi->pdbits);

        // Map from Kernel VM area (0xD0000000 - 0xF0000000 (or 0xE0000000 on SHx))
        DoMappPD (pptbl, g_ppdirNK,
                        (VM_NKVM_BASE - VM_SHARED_HEAP_BASE) >> 22,
                        VA2PDIDX (VM_NKVM_BASE),
                        VA2PDIDX (VM_CPU_SPECIFIC_BASE),
                        ppmi->pdbits);

#ifdef ARM
        ARMPteUpdateBarrier (&pptbl->pte[0], PAGECOUNT (VM_PD_DUP_SIZE));
#endif

    }

    UnlockVM (pprc);

    ppmi->fIsKernel = TRUE;

    return dwRet;
}

static DWORD VMMapAppPD (PPROCESS pprc, PPROCMEMINFO ppmi)
{
    DWORD dwRet = 0;
    PPROCESS pprcDest = pVMProc;

    if (Lock2VM (pprc, pprcDest)) {

        DEBUGCHK (pprc->ppdir && pprcDest->ppdir);

        if (dwRet = DoVMReserve (pprcDest, VM_PD_DUP_ADDR, PAGECOUNT (VM_PD_DUP_SIZE), 0, 0)) {
            PPAGETABLE pptbl = Entry2PTBL (pprcDest->ppdir->pte[VA2PDIDX(dwRet)]);
            DEBUGCHK (pptbl);

            // Dup from user address area (0 - 0x6fc00000)
            DoMappPD (pptbl, pprc->ppdir,
                            0,
                            0,
                            VA2PDIDX (VM_PD_DUP_ADDR),
                            ppmi->pdbits);
#ifdef ARM
            ARMPteUpdateBarrier (&pptbl->pte[0], PAGECOUNT (VM_PD_DUP_SIZE));
#endif
        }

        UnlockVM (pprc);
        UnlockVM (pprcDest);
    }

    ppmi->dwExeEnd = (DWORD) pprc->BasePtr + pprc->e32.e32_vsize;

    return dwRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMDupPageDirectory: map page directory of a process. The mapping of the page-directory
//                     is read-only.
//
// NOTE: For security purpose, we force the mapping to be at a fixed addresss, such that the page
//       protection cannot be changed. As a result, there can be only one mapping at any given time.
//
LPVOID VMMapPD (DWORD dwProcId, PPROCMEMINFO ppmi)
{
    DWORD dwRet = 0;
    PROCMEMINFO pmiLocal = {0};

    if (dwProcId == g_pprcNK->dwId) {
        dwRet = VMMapKernelPD (&pmiLocal);
    } else {
        PHDATA phd = LockHandleData ((HANDLE) dwProcId, g_pprcNK);
        PPROCESS pprc = GetProcPtr (phd);

        if (pprc) {
            dwRet = VMMapAppPD (pprc, &pmiLocal);
        }
        UnlockHandleData (phd);
    }

    if (dwRet) {
        CeSafeCopyMemory (ppmi, &pmiLocal, sizeof (pmiLocal));
    }

    return (LPVOID) dwRet;
}

BOOL VMUnmapPD (LPVOID pDupPD)
{
    return (VM_PD_DUP_ADDR == (DWORD) pDupPD)
        ? VMFreeAndRelease (pVMProc, pDupPD, VM_PD_DUP_SIZE)
        : FALSE;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMFreeExcessStacks: (internal only) free excess cached stack
//
void VMFreeExcessStacks (long nMaxStks)
{
    PSTKLIST pStkLst;

    while ((g_nStackCached > nMaxStks)
        && (pStkLst = InterlockedPopList (&g_pStkList))) {
        // decrement of count must occur after pop
        DEBUGMSG (ZONE_MEMORY, (L"VMFreeExcessStacks %8.8lx, size %8.8lx\r\n", pStkLst->pStkBase, pStkLst->cbStkSize));
        DEBUGCHK (KRN_STACK_SIZE == pStkLst->cbStkSize);
        InterlockedDecrement (&g_nStackCached);
        VERIFY (VMFreeAndRelease (g_pprcNK, pStkLst->pStkBase, pStkLst->cbStkSize));
        FreeMem (pStkLst, HEAP_STKLIST);
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMCacheStack: (internal only) cache a stack for future use
// NOTE: callable inside KCall
//
void VMCacheStack (PSTKLIST pStkList, DWORD dwBase, DWORD cbSize)
{
    pStkList->cbStkSize = cbSize;
    pStkList->pStkBase  = (LPVOID)dwBase;
    // must increment cnt before push
    InterlockedIncrement (&g_nStackCached);
    InterlockedPushList (&g_pStkList, pStkList);
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMFreeStack: (internal only) free or cache a stack for later use
//
void VMFreeStack (PPROCESS pprc, DWORD dwBase, DWORD cbSize)
{
    PSTKLIST pStkList;
    if (   (pprc != g_pprcNK)
        || (KRN_STACK_SIZE != cbSize)
        || (g_nStackCached >= MAX_STK_CACHE)
        || !(pStkList = AllocMem (HEAP_STKLIST))) {
        VMFreeAndRelease (pprc, (LPVOID) dwBase, cbSize);
    } else {
        VMCacheStack (pStkList, dwBase, cbSize);
    }
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMCreateStack: (internal only) Create a stack and commmit the bottom-most page
//
LPBYTE
VMCreateStack (
    PPROCESS pprc,
    DWORD cbSize
    )
{
    PSTKLIST pStkLst  = NULL;
    LPBYTE   pKrnStk, pUsrStk;

    DEBUGCHK (!(cbSize & VM_BLOCK_OFST_MASK));   // size must be multiple of 64K

    // try to use cached stacks
    if ((KRN_STACK_SIZE == cbSize) && (pStkLst = InterlockedPopList (&g_pStkList))) {
        InterlockedDecrement (&g_nStackCached);
    }

    if (pStkLst) {
        // got an old stack
        pKrnStk = pStkLst->pStkBase;

        DEBUGCHK (pKrnStk);
        DEBUGCHK (!((DWORD)pKrnStk & VM_BLOCK_OFST_MASK));

        // free pStkLst
        FreeMem (pStkLst, HEAP_STKLIST);

    // no old stack available, create a new one
    } else if (pKrnStk = (LPBYTE) VMReserve (g_pprcNK, cbSize, MEM_AUTO_COMMIT, 0)) {

        if (((DWORD) pKrnStk < MAX_STACK_ADDRESS)
            && VMCommit (g_pprcNK, pKrnStk + cbSize - VM_PAGE_SIZE, VM_PAGE_SIZE, PAGE_READWRITE, PM_PT_ZEROED)) {
            // initialize stack
            MDInitStack (pKrnStk, cbSize);
        } else {
            // commit stack failed - out of memory
            VMFreeAndRelease (g_pprcNK, pKrnStk, cbSize);
            pKrnStk = NULL;
        }
    }

    // initialize return value
    pUsrStk = pKrnStk;

    if (pUsrStk) {

        // NOTE: NEVER ACCESS *pUsrStk in this function, or it can cause cache in-consistency

        if ((pprc == g_pprcNK) || (pUsrStk = VMReserve (pprc, cbSize, MEM_AUTO_COMMIT, 0))) {

            LPDWORD  tlsKrn = TLSPTR (pKrnStk, cbSize);
            LPDWORD  tlsUsr = TLSPTR (pUsrStk, cbSize);

            if (pStkLst) {
                // zero TLS/pretls if old stack
                if (pprc == g_pprcNK) {
                    memset (tlsKrn-PRETLS_RESERVED, 0, sizeof(DWORD)*(TLS_MINIMUM_AVAILABLE+PRETLS_RESERVED));
                } else {
                    // security - zero the stack page
                    ZeroPage ((LPVOID) ((DWORD) tlsKrn & ~VM_PAGE_OFST_MASK));
                    tlsKrn[PRETLS_STACKBOUND] = (DWORD) tlsUsr & ~VM_PAGE_OFST_MASK;
                    MDInitStack (pKrnStk, cbSize);
                }
            } else {
                tlsKrn[PRETLS_STACKBOUND] = (DWORD) tlsUsr & ~VM_PAGE_OFST_MASK;
            }

            tlsKrn[PRETLS_STACKBASE]  = (DWORD) pUsrStk;
            tlsKrn[PRETLS_STACKSIZE]  = cbSize;
            tlsKrn[TLSSLOT_RUNTIME]   = (DWORD) CRTGlobFromTls (tlsUsr);
        }

        if (pUsrStk != pKrnStk) {
            if (pUsrStk) {
                VERIFY (VMFastCopy (pprc, (DWORD) pUsrStk + cbSize - VM_PAGE_SIZE, g_pprcNK, (DWORD) pKrnStk + cbSize - VM_PAGE_SIZE, VM_PAGE_SIZE, PAGE_READWRITE));
            }
            // we don't need to write-back cache here because the call to VMFreeAndRelease will write-back and discard
            VMFreeAndRelease (g_pprcNK, pKrnStk, cbSize);
        }
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"VMCreateStack returns %8.8lx\r\n", pUsrStk));
    return pUsrStk;
}

static DWORD _GetPFN (PPROCESS pprc, LPCVOID pAddr)
{
    DWORD    dwPFN = INVALID_PHYSICAL_ADDRESS;
    DWORD    dwPDEntry = GetPDEntry (pprc->ppdir, pAddr);
    PPAGETABLE pptbl;

    DEBUGCHK (!((DWORD) pAddr & VM_PAGE_OFST_MASK));

    if (IsSectionMapped (dwPDEntry)) {
        dwPFN = PFNfromSectionEntry (dwPDEntry) + SectionOfst (pAddr);

    } else if (pptbl = Entry2PTBL (dwPDEntry)) {
        dwPFN = PFNfromEntry (pptbl->pte[VA2PT2ND(pAddr)]);
    }
    return dwPFN;
}

//
// get physical page number of a virtual address of a process
//
DWORD GetPFNOfProcess (PPROCESS pprc, LPCVOID pAddr)
{
    if ((DWORD) pAddr >= VM_SHARED_HEAP_BASE) {
        pprc = g_pprcNK;
    } else if (!pprc) {
        pprc = pVMProc;
    }
    return _GetPFN (pprc, pAddr);
}



//
// get physical page number of a virtual address
//
DWORD GetPFN (LPCVOID pAddr)
{
    return _GetPFN (((DWORD) pAddr >= VM_SHARED_HEAP_BASE)? g_pprcNK : pVMProc, pAddr);
}


LPVOID GetKAddrOfProcess (PPROCESS pprc, LPCVOID pAddr)
{
    DWORD dwOfst = (DWORD)pAddr & VM_PAGE_OFST_MASK;
    DWORD dwPfn = GetPFNOfProcess (pprc, (LPVOID) ((DWORD) pAddr & -VM_PAGE_SIZE));

    if (dwPfn && (pAddr = Pfn2Virt (dwPfn))) {
        pAddr = (LPBYTE)pAddr + dwOfst;
    } else {
        pAddr = NULL;
    }

    return (LPVOID) pAddr;
}

LPVOID GetKAddr (LPCVOID pAddr)
{
    return GetKAddrOfProcess (NULL, pAddr);
}

//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------
//
// Shared heap support (K-Mode R/W, U-Mode R/O), range (VM_SHARED_HEAP_BASE - 0x7fffffff)
//
//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------
LPVOID NKVirtualSharedAlloc (LPVOID lpvAddr, DWORD cbSize, DWORD fdwAction)
{
    DWORD dwErr = 0;

    DEBUGMSG (ZONE_VIRTMEM, (L"NKVirtualShareAlloc: %8.8lx %8.8lx %8.8lx\r\n", lpvAddr, cbSize, fdwAction));

    if (!fdwAction || (~(MEM_COMMIT|MEM_RESERVE) & fdwAction)) {
        dwErr = ERROR_INVALID_PARAMETER;
    } else if (!lpvAddr) {
        // NULL lpvAddr, always RESERVE
        fdwAction |= MEM_RESERVE;
    } else if (!IsInSharedHeap ((DWORD) lpvAddr) || (MEM_COMMIT != fdwAction)) {
        // committing, but address not in shared heap
        dwErr = ERROR_INVALID_PARAMETER;
    }

    if (!dwErr) {

        if (MEM_RESERVE & fdwAction) {
            DEBUGCHK (!lpvAddr);
            lpvAddr = VMReserve (g_pprcNK, cbSize, 0, VM_SHARED_HEAP_BASE);
        }

        if (!lpvAddr) {
            dwErr = ERROR_NOT_ENOUGH_MEMORY;

        } else {
            if ((MEM_COMMIT & fdwAction)
                && !VMAlloc (g_pprcNK, lpvAddr, cbSize, MEM_COMMIT, PAGE_READWRITE)) {
                // failed committing memory
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            }

            if (MEM_RESERVE & fdwAction) {

                if (!dwErr
                    && !VMAddAllocToList (g_pprcNK, lpvAddr, cbSize, NULL)) {
                    dwErr = ERROR_NOT_ENOUGH_MEMORY;
                }

                if (dwErr) {
                    // failed committing memory, release VM
                    VERIFY (VMFreeAndRelease (g_pprcNK, lpvAddr, cbSize));
                }
            }
        }

    }

    if (dwErr) {
        NKSetLastError (dwErr);
    }

    DEBUGMSG (ZONE_VIRTMEM || dwErr, (L"NKVirtualShareAlloc returns: %8.8lx, GetLastError() = %8.8lx\r\n", dwErr? NULL : lpvAddr, dwErr));
    return dwErr? NULL : lpvAddr;
}


//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------
//
// Memory-Mapped file support
//
//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------

typedef struct {
    DWORD   cDirtyPages;
    LPBYTE  pAddr;
    PFN_MapFlushRecordPage pfnRecordPage;
    FlushParams* pParams;
} VMMapCountStruct, *PVMMapCountStruct;


static BOOL MapCountDirtyPages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVMMapCountStruct pvmcs = (PVMMapCountStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    BOOL  fRet    = TRUE;

    if (IsPageWritable (dwEntry)) {
        // pfnRecordPage may modify the page, so leave it R/W until after the call
        fRet = pvmcs->pfnRecordPage (pvmcs->pParams, pvmcs->pAddr, pvmcs->cDirtyPages);
        *pdwEntry = ReadOnlyEntry (dwEntry);
        pvmcs->cDirtyPages++;
    }

    // advance the address
    pvmcs->pAddr += VM_PAGE_SIZE;

    return fRet;
}


//
// VMMapClearDirtyPages - Call a mapfile callback function on every dirty page
//                        in the range, and mark them read-only before returning.
//                        Returns the number of pages that were cleared.
//
DWORD VMMapClearDirtyPages (PPROCESS pprc, LPBYTE pAddr, DWORD cbSize,
                            PFN_MapFlushRecordPage pfnRecordPage, FlushParams* pParams)
{
    VMMapCountStruct vmcs = { 0, pAddr, pfnRecordPage, pParams };
    DWORD cPages = PAGECOUNT (cbSize);

    DEBUGCHK (!((DWORD) pAddr & VM_PAGE_OFST_MASK));

    if (LockVM (pprc)) {

        if (VMIsPagesLocked (pprc, (DWORD) pAddr, cPages)) {
            DEBUGMSG (ZONE_MAPFILE, (L"VMMapClearDirtyPages: Cannot clear Addr=%8.8lx, %u pages because the pages are locked\r\n", pAddr, cPages));
            KSetLastError (pCurThread, ERROR_SHARING_VIOLATION);
            vmcs.cDirtyPages = (DWORD) -1;

        } else {
            Enumerate2ndPT (pprc->ppdir, (DWORD) pAddr, cPages, FALSE,
                            MapCountDirtyPages, &vmcs);

            // flush TLB if there is any dirty page
            if (vmcs.cDirtyPages) {
                InvalidatePages (pprc, (DWORD) pAddr, cPages);
            }
        }

        UnlockVM (pprc);
    }

    return vmcs.cDirtyPages;
}


// Enumeration state for VMMapCopy
typedef struct {
    PFSMAP  pfsmap;
    LPBYTE  pAddr;
    ULARGE_INTEGER liFileOffset;
} VMMapCopyStruct, *PVMMapCopyStruct;

static BOOL MapCopyPage (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVMMapCopyStruct pvmcs = (PVMMapCopyStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    BOOL  fRet = TRUE;

    if (IsPageCommitted (dwEntry)) {
        // Use count=1 for the page because this procedure is only used for
        // non-pageable mappings, which need to hold all the pages until the
        // mapfile is destroyed, rather than releasing them when the last view
        // is destroyed.
        fRet = MapVMAddPage (pvmcs->pfsmap, pvmcs->liFileOffset, pvmcs->pAddr, 1);
    } else {
        DEBUGCHK (0);  // Should already be committed
        fRet = FALSE;
    }

    // advance the address
    pvmcs->pAddr += VM_PAGE_SIZE;
    pvmcs->liFileOffset.QuadPart += VM_PAGE_SIZE;

    return fRet;
}


//
// VMMapCopy: VirtualCopy from process VM to mapfile page tree.
//
BOOL VMMapCopy (PFSMAP pfsmapDest, PPROCESS pprcSrc, LPVOID pAddrPrc, DWORD cbSize)
{
    VMMapCopyStruct vmcs;
    DWORD cPages = PAGECOUNT (cbSize);
    BOOL  fRet = FALSE;

    DEBUGCHK (!((DWORD)pAddrPrc & VM_PAGE_OFST_MASK));

    DEBUGMSG (ZONE_MAPFILE, (L"VMMapCopy: Copy from %8.8lx of pid = %8.8lx, size %8.8lx to pfsmap = %8.8lx\r\n",
                pAddrPrc, pprcSrc->dwId, cbSize, pfsmapDest));

    // Cannot acquire the mapfile VM CS after acquiring the process VM CS.
    // Since MapCopyPage calls back into the mapfile VM code, the caller of
    // VMMapCopy must already have acquired the mapfile VM CS.
    DEBUGCHK (OwnCS (&pfsmapDest->pgtree.csVM));

    vmcs.pfsmap = pfsmapDest;
    vmcs.pAddr = pAddrPrc;
    vmcs.liFileOffset.QuadPart = 0;  // Copying entire file, start at beginning

    if (LockVM (pprcSrc)) {
        fRet = Enumerate2ndPT (pprcSrc->ppdir, (DWORD) pAddrPrc, cPages, FALSE,
                               MapCopyPage, &vmcs);
        UnlockVM (pprcSrc);

        DEBUGCHK (!fRet || (vmcs.liFileOffset.QuadPart == cbSize));
    }

    return fRet;
}


// Enumeration state for VMMapDecommit
typedef struct {
    PFSMAP   pfsmap;
    PPROCESS pprc;
    LPBYTE   pAddr;
    ULARGE_INTEGER liFileOffset;
    BOOL     fFreeAll;
    BOOL     fHasLockedPages;
    BOOL     fCommittedPagesRemaining;
} VMMapDecommitStruct, *PVMMapDecommitStruct;

static BOOL MapDecommitPage (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVMMapDecommitStruct pvmds = (PVMMapDecommitStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    BOOL  fRet = TRUE;

    if (IsPageCommitted (dwEntry)) {
        if (pvmds->fFreeAll || !IsPageWritable (dwEntry)) {
            // Don't decommit if it's locked
            if (pvmds->fHasLockedPages
                && VMIsPagesLocked (pvmds->pprc, (DWORD) pvmds->pAddr, 1)) {
                DEBUGMSG (ZONE_MAPFILE, (L"VMMapClearDirtyPages: Skipping decommit on locked page at Addr=%8.8lx\r\n", pvmds->pAddr));
                pvmds->fCommittedPagesRemaining = TRUE;
            } else {
                // DecommitPages removes the page from the view.  If this is the last
                // view it was paged into, MapVMFreePage removes the page from the page
                // tree and gives it back to the page pool.
                if (!DecommitPages (pdwEntry, (LPVOID) VM_PAGER_MAPPER)
                    || !MapVMFreePage (pvmds->pfsmap, pvmds->liFileOffset)) {
                    // Failed
                    DEBUGCHK (0);
                    pvmds->fCommittedPagesRemaining = TRUE;
                    fRet = FALSE;
                }
            }
        } else {
            // Writable page is still committed
            pvmds->fCommittedPagesRemaining = TRUE;
        }
    }

    // advance the address
    pvmds->pAddr += VM_PAGE_SIZE;
    pvmds->liFileOffset.QuadPart += VM_PAGE_SIZE;

    return fRet;
}


//
// VMMapDecommit: Decommit view memory from process VM.  Will discard only
// read-only pages, or all, depending on fFreeAll.  Maintains mapfile page tree
// page refcounts.  Returns TRUE if all pages have been decommitted and FALSE
// if there are committed pages remaining.  (Note, however, that unless care is
// taken to prevent new commits after the decommit, the commit status could be
// invalidated immediately.)
//
BOOL
VMMapDecommit (
    PFSMAP   pfsmap,
    const ULARGE_INTEGER* pliFileOffset,
    PPROCESS pprc,
    LPVOID   pAddrPrc,
    DWORD    cbSize,
    BOOL     fFreeAll       // TRUE: discard all, FALSE: discard only R/O pages
    )
{
    VMMapDecommitStruct vmds;
    DWORD cPages = PAGECOUNT (cbSize);
    BOOL  fRet = FALSE;

    DEBUGCHK (!((DWORD)pAddrPrc & VM_PAGE_OFST_MASK));

    DEBUGMSG (ZONE_MAPFILE, (L"VMMapDecommit: Decommit from %8.8lx of pid = %8.8lx, size %8.8lx in pfsmap = %8.8lx\r\n",
                             pAddrPrc, pprc->dwId, cbSize, pfsmap));

    // Cannot acquire the mapfile VM CS after acquiring the process VM CS.
    // Since MapDecommitPage calls back into the mapfile VM code, the caller of
    // VMMapDecommit must already have acquired the mapfile VM CS.
    DEBUGCHK (OwnCS (&pfsmap->pgtree.csVM));

    vmds.pfsmap = pfsmap;
    vmds.pprc = pprc;
    vmds.pAddr = pAddrPrc;
    vmds.liFileOffset.QuadPart = pliFileOffset->QuadPart;
    vmds.fFreeAll = fFreeAll;
    vmds.fCommittedPagesRemaining = FALSE;

    if (LockVM (pprc)) {
        // write-back all dirty pages
        OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_WRITEBACK);

        // If any pages are locked, we leave those but decommit the rest.
        // fHasLockedPages skips walking the lock list on each page if none are locked.
        vmds.fHasLockedPages = VMIsPagesLocked (pprc, (DWORD) pAddrPrc, cPages);

        fRet = Enumerate2ndPT (pprc->ppdir, (DWORD) pAddrPrc, cPages, FALSE,
                               MapDecommitPage, &vmds);
        InvalidatePages (pprc, (DWORD) pAddrPrc, cPages);
        UnlockVM (pprc);

        DEBUGCHK (!fRet || (vmds.pAddr == (LPBYTE) PAGEALIGN_UP ((DWORD) pAddrPrc + cbSize)));

        if (vmds.fCommittedPagesRemaining) {
            // There are still locked pages or dirty pages remaining
            fRet = FALSE;
            DEBUGCHK (!fFreeAll);
        }
    }

    return fRet;
}


//
//
// VMMapTryExisting: used for paging, change VM attributes if page already committed
//
// NOTE: caller must have verified that the addresses are valid, and writeable if fWrite is TRUE
//
VMMTEResult VMMapTryExisting (PPROCESS pprc, LPVOID pAddrPrc, DWORD fProtect)
{
    PPAGEDIRECTORY ppdir = LockVM (pprc);
    VMMTEResult result = VMMTE_FAIL;

    DEBUGMSG (ZONE_MAPFILE && ZONE_PAGING,
              (L"VMMapTryExisting: %8.8lx %8.8lx (%8.8lx)\r\n",
               pprc, pAddrPrc, fProtect));

    if (ppdir) {
        PPAGETABLE pptbl = Entry2PTBL (ppdir->pte[VA2PDIDX(pAddrPrc)]);
        DWORD idx2nd = VA2PT2ND (pAddrPrc);
        if (pptbl) {
            DWORD dwEntry = pptbl->pte[idx2nd];

            if (IsPageCommitted (dwEntry)) {
                // page already committed. Change protection if needed
                if (PAGE_READWRITE & fProtect) {
                    if (IsPageWritable (dwEntry)) {
                        result = VMMTE_ALREADY_EXISTS;
                    } else {
                        pptbl->pte[idx2nd] = MakeCommittedEntry (PFNfromEntry (dwEntry), PageParamFormProtect (fProtect, (DWORD) pAddrPrc));
#ifdef ARM
                        ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
                        InvalidatePages (pprc, (DWORD) pAddrPrc, 1);
                        result = VMMTE_SUCCESS;
                    }
                } else {
                    // other thread paged in the page before us. Do nothing
                    DEBUGMSG (ZONE_MAPFILE || ZONE_PAGING,
                              (L"VMMapTryExisting: Other thread paged-in page %8.8lx\r\n",
                               pAddrPrc));
                    result = VMMTE_ALREADY_EXISTS;
                }
            }
        }
        UnlockVM (pprc);
    }

    DEBUGMSG ((VMMTE_FAIL == result) && ZONE_MAPFILE && ZONE_PAGING,
              (L"VMMapTryExisting - no existing page at %8.8lx\r\n", pAddrPrc));

    return result;

}


//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------
//
// static mapping support
//
//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------

//
// Find out if given physical base and size match with any existing static mapping
//
LPVOID FindStaticMapping (DWORD dwPhysBase, DWORD dwSize)
{
    LPVOID MappedVirtual = NULL;
    PSTATICMAPPINGENTRY pItem = g_StaticMappingList;
    DWORD dwPhysEnd = dwPhysBase + (dwSize >> 8);

    // check for overflow
    if (dwPhysEnd < dwPhysBase
        || dwPhysEnd < dwSize) {
        return NULL;
    }

    // scan the list
    while (pItem) {

        // check for range match
        if ((dwPhysBase >= pItem->dwPhysStart)
            && (dwPhysEnd <= pItem->dwPhysEnd)) {
            MappedVirtual = (LPVOID) ((DWORD)pItem->lpvMappedVirtual + ((dwPhysBase - pItem->dwPhysStart) << 8));
            break;
        }

        pItem = pItem->pNext;
    }

    return MappedVirtual;
}

//
// Add a entry to the head of the static mapping list
//
void AddStaticMapping(DWORD dwPhysBase, DWORD dwSize, LPVOID MappedVirtual)
{
    PSTATICMAPPINGENTRY pvItem = (PSTATICMAPPINGENTRY) AllocMem (HEAP_STATICMAPPING_ENTRY);
    DWORD dwPhysEnd = dwPhysBase + (dwSize >> 8);

    // check for overflow
    if (dwPhysEnd < dwPhysBase
        || dwPhysEnd < dwSize) {
        return;
    }

    if (pvItem) {
        pvItem->lpvMappedVirtual = MappedVirtual;
        pvItem->dwPhysStart = dwPhysBase;
        pvItem->dwPhysEnd = dwPhysEnd;
        InterlockedPushList (&g_StaticMappingList, pvItem);
    }

    return;
}

//-----------------------------------------------------------------------------------------------------------------
//
// NKCreateStaticMapping: Create a static mapping
//
LPVOID
NKCreateStaticMapping(
    DWORD dwPhysBase,
    DWORD cbSize
    )
{
    LPVOID pRet = NULL;

    DEBUGMSG (ZONE_VIRTMEM, (L"CreateStaticMapping: %8.8lx %8.8lx\r\n", dwPhysBase, cbSize));

    cbSize = PAGEALIGN_UP(cbSize);
    DEBUGCHK(cbSize);

    if (!IsPageAligned ((dwPhysBase<<8))) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
#if defined (ARM) || defined (x86)
    } else if (pRet = Pfn2VirtUC (PFNfrom256(dwPhysBase))) {
        // do nothing, pRet already set
#else
    } else if (dwPhysBase < 0x200000) {
        pRet = Pfn2VirtUC (PFNfrom256(dwPhysBase));
#endif
    } else if (!(pRet = FindStaticMapping(dwPhysBase, cbSize))){
        // not in the cached static mapping list
        pRet = VMReserve (g_pprcNK, cbSize, 0, 0);

        if (pRet) {
            if(!VMCopy (g_pprcNK, (DWORD) pRet, NULL, dwPhysBase, cbSize, PAGE_PHYSICAL|PAGE_NOCACHE|PAGE_READWRITE)) {
                VMFreeAndRelease (g_pprcNK, pRet, cbSize);
                pRet = NULL;

            } else if (g_pprcNK->csVM.hCrit){
                // add to the static mapping list as long as kernel heap is initialized
                AddStaticMapping(dwPhysBase, cbSize, pRet);
            }
        }

    }
    DEBUGMSG (ZONE_VIRTMEM, (L"CreateStaticMapping: returns %8.8lx\r\n", pRet));

    return pRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// NKDeleteStaticMapping: Delete a static mapping
//
BOOL
NKDeleteStaticMapping (
    LPVOID pVirtAddr,
    DWORD dwSize
    )
{
    BOOL fRet = IsKernelVa (pVirtAddr)
            || (   IsInKVM ((DWORD) pVirtAddr)
                && IsInKVM ((DWORD) pVirtAddr + dwSize));
    /*
                //
                // For b/c reasons, not releasing any static mapping allocation
                // since most existing drivers do not call DeleteStaticMapping
                // and also static mapping allocations are not refcounted
                // currently.
                //
                && VMFreeAndRelease (g_pprcNK, pVirtAddr, dwSize));
    */

    DEBUGMSG (!fRet, (L"!ERROR: DeleteStaticMapping (%8.8lx %8.8lx) Failed\r\n", pVirtAddr, dwSize));
    return fRet;
}
