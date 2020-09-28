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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++

Module Name:

    OsAxsProtocol.h

Abstract:

    This file details the protocol used to communicate between the
    host side OsAccess service and the target side OsAccess service.

--*/

#pragma once
#ifndef OSAXS_PROTOCOL_H
#define OSAXS_PROTOCOL_H

#include "osaxs_common.h"

#pragma pack(push, 4)

// OS Access error codes
#define OSAXS_E_PROTOCOLVERSION     MAKE_HRESULT(1, FACILITY_WINDOWS_CE, 0x4F00)   // Notify OsAxs Kdbg code that the version is incorrect.
#define OSAXS_E_APINUMBER           MAKE_HRESULT(1, FACILITY_WINDOWS_CE, 0x4F01)   // Bad api number was passed to OS Access
#define OSAXS_E_VANOTMAPPED         MAKE_HRESULT(1, FACILITY_WINDOWS_CE, 0x4F02)   // Unable to find the Virtual address in the address table
#define OSAXS_E_INVALIDTHREAD       MAKE_HRESULT(1, FACILITY_WINDOWS_CE, 0x4F03)   // Bad thread pointer.

// OS Access Protocol numbers corresponding to target-host functionality.
enum
{
    OSAXS_PROTOCOL_VERSION_EXDI2            = 0,          // Windows CE 5.00
    OSAXS_PROTOCOL_VERSION_WATSON           = 1,          // Windows CE 5.01
    OSAXS_PROTOCOL_VERSION_NEW_VM           = 2,          // Windows CE 6.00

    // Current protocol version.
    OSAXS_PROTOCOL_LATEST_VERSION           = OSAXS_PROTOCOL_VERSION_NEW_VM,

    // Earliest protocol version that the target side can handle.
    OSAXS_PROTOCOL_LATEST_VERSION_TSBC_WITH = OSAXS_PROTOCOL_VERSION_EXDI2,

    // Earliest protocol version that the host side can handle.
    OSAXS_PROTOCOL_LATEST_VERSION_HSBC_WITH = OSAXS_PROTOCOL_VERSION_EXDI2,
};

// HI-word = protocol version, LO-word = api number
#define MAKE_API_NUMBER(ver, apinum) ((((ver) & 0xFFFF) << 16) | ((apinum) & 0xFFFF))


// OS Access API Numbers
enum
{
    OSAXS_API_GET_FLEXIPTMINFO              = MAKE_API_NUMBER(OSAXS_PROTOCOL_VERSION_EXDI2, 0x0001),
    OSAXS_API_GET_EXCEPTION_REGISTRATION    = MAKE_API_NUMBER(OSAXS_PROTOCOL_VERSION_EXDI2, 0x0009),
    OSAXS_API_GET_WATSON_DUMP_FILE_COMPAT   = 0x000A,            // Compat here because older versions had this number hardcoded.
    OSAXS_API_GET_WATSON_DUMP_FILE          = MAKE_API_NUMBER(OSAXS_PROTOCOL_VERSION_WATSON, 0x0001),
    OSAXS_API_SWITCH_PROCESS_VIEW           = MAKE_API_NUMBER(OSAXS_PROTOCOL_VERSION_NEW_VM, 0x0001),
    OSAXS_API_TRANSLATE_ADDRESS             = MAKE_API_NUMBER(OSAXS_PROTOCOL_VERSION_NEW_VM, 0x0002),
    OSAXS_API_TRANSLATE_HPCI                = MAKE_API_NUMBER(OSAXS_PROTOCOL_VERSION_NEW_VM, 0x0003),
    OSAXS_API_CALLSTACK                     = MAKE_API_NUMBER(OSAXS_PROTOCOL_VERSION_NEW_VM, 0x0004),
    OSAXS_API_GET_HDATA                     = MAKE_API_NUMBER(OSAXS_PROTOCOL_VERSION_NEW_VM, 0x0005),
};


//
// OSAXS_API_GET_EXCEPTION_REGISTRATION
//
typedef struct OSAXS_API_GET_EXCEPTION_REGISTRATION
{
    DWORD rgdwBuf[2];
} OSAXS_EXCEPTION_REGISTRATION;


//
// OSAXS_API_WATSON_DUMP_OTF
//
typedef struct OSAXS_API_WATSON_DUMP_OTF
{
    DWORD dwDumpType;            // Dump type: 1 = Context, 2 = System, 3 = Complete  (on return contains actual dump type)
    DWORD dwSegmentBufferSize;   // Maximum buffer size for segment of data to return;
    DWORD64 dw64MaxDumpFileSize; // Maximum dump file size the host can accept (on return contains the actual dump file size)
    DWORD64 dw64SegmentIndex;    // Returns index of current segment within entire dump file
    BOOL fDumpfileComplete;      // Indicate dump generation complete 
    DWORD dwPad1[4];
} OSAXS_API_WATSON_DUMP_OTF;


//
// OSAXS_API_TRANSLATE_ADDRESS
//
typedef struct _OSAXS_TRANSLATE_ADDRESS
{
    DWORD dwProcessHandle;
    DWORD dwPad0;
    ULONG64 Address;
    BOOL fReturnKVA;
    DWORD dwPad1;
    ULONG64 PhysicalAddrOrKVA;
} OSAXS_TRANSLATE_ADDRESS;

typedef struct _OSAXS_QUERY_HANDLE
{
    DWORD dwProcessHandle;
    DWORD dwHandle;
} OSAXS_QUERY_HANDLE;


typedef struct _OSAXS_CALLSTACK
{
    DWORD hThread;
    DWORD FrameStart;
    DWORD FramesToRead;
    DWORD FramesReturned;
} OSAXS_CALLSTACK;


typedef struct _OSAXS_CALLSTACK_FRAME
{
    ULONG32 ReturnAddr;
    ULONG32 FramePtr;
    ULONG32 ProcessId;
    ULONG32 Params[4];
} OSAXS_CALLSTACK_FRAME;


//
// OsAxs Protocol Structure.  This structure is padded out to resemble
// the DBGKD_COMMAND structure.
//
typedef struct OSAXS_COMMAND
{
    DWORD dwApi;
    DWORD dwVersion;
    DWORD dwPad0;
    HRESULT hr;
    DWORD dwPad1[4];
    union
    {
        FLEXI_REQUEST FlexiReq;
        ULONG64 Addr;
        OSAXS_EXCEPTION_REGISTRATION ExReg;
        OSAXS_API_WATSON_DUMP_OTF GetWatsonOTF;
        OSAXS_TRANSLATE_ADDRESS TranslateAddress;
        OSAXS_QUERY_HANDLE Handle;
        OSAXS_CALLSTACK CallStack;
        BYTE rgbPad2[48];
    } u;
    
} OSAXS_COMMAND;

#pragma pack(pop)

#endif
