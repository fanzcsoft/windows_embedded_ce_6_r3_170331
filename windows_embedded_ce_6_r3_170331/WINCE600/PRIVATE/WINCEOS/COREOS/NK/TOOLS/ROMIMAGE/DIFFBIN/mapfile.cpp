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
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

	mapfile.cpp

Abstract:

    Class for a memory mapped file

Author:

	Scott Shell (ScottSh)

Environment:

	Win32

Revision History:

    30-Jun-2001 Scott Shell (ScottSh)       Created    

-------------------------------------------------------------------*/
#include <windows.h>
#include "mapfile.h"

CMappedFile::CMappedFile(char *szFilename) : m_hFile(INVALID_HANDLE_VALUE),
                                             m_hMapping(NULL),
                                             m_strFilename(szFilename),
                                             m_pvData(NULL),
                                             m_dwMapSize(0)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
} /* CMappedFile::CMappedFile()
   */



CMappedFile::~CMappedFile()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    Close();

    m_strFilename.erase();

} /* CMappedFile::~CMappedFile()
   */

HRESULT
CMappedFile::Close()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr      = NOERROR;
    
    if(m_pvData) {
        UnmapViewOfFile(m_pvData);
        m_pvData = NULL;
    }

    if(m_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }

    if(m_hMapping != NULL) {
        CloseHandle(m_hMapping);
        m_hMapping = NULL;
    }

    return hr;

} /* CMappedFile::Close()
   */

HRESULT
CMappedFile::Open(DWORD dwAccess, DWORD dwCreationDisposition, DWORD dwShareMode)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr      = NOERROR;
    DWORD           dwProtect;
    DWORD           dwMapAccess;

    // Ignore failures in close...
    Close();

    m_hFile = CreateFile(m_strFilename.c_str(), dwAccess, dwShareMode, NULL, dwCreationDisposition, 0, NULL);

    CBR(m_hFile != INVALID_HANDLE_VALUE);

    if(m_dwMapSize == 0) {
        m_dwMapSize = GetFileSize(m_hFile, NULL);
    }

    dwProtect = (dwAccess & GENERIC_WRITE) ? PAGE_READWRITE : PAGE_READONLY;

    m_hMapping = CreateFileMapping(m_hFile, NULL, dwProtect, 0, m_dwMapSize, NULL);

    CBR(m_hMapping != NULL);

    dwMapAccess = (dwAccess & GENERIC_WRITE) ? FILE_MAP_WRITE : FILE_MAP_READ;
    
    m_pvData = MapViewOfFileEx(m_hMapping, dwMapAccess, 0, 0, m_dwMapSize, NULL);

    CBR(m_pvData != NULL);

Error:
    return HRESULT_FROM_WIN32(GetLastError());

} /* CMappedFile::Open()
   */
