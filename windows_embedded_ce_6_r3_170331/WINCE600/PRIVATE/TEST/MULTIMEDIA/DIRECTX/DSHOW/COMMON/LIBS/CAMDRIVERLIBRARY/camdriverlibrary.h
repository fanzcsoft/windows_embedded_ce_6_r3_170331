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
#ifndef CAMDRIVERLIBRARY_H
#define CAMDRIVERLIBRARY_H

#include <windows.h>
#include <tchar.h>
#include <pnp.h>
#include "..\DriverEnumerator\DriverEnumerator.h"

typedef class CCamDriverLibrary : public CDriverEnumerator
{
    public:
        CCamDriverLibrary();
        ~CCamDriverLibrary();

        HRESULT SetupNULLCameraDriver();
        int GetNULLCameraDriverIndex();
        HRESULT AddNULLCameraRegKey(TCHAR *tszKey, TCHAR *tszData);
        HRESULT AddNULLCameraRegKey(TCHAR *tszKey, DWORD tszData);

    private:
        int m_nNullDriverIndex;
}CAMDRIVERLIBRARY, *PCAMDRIVERLIBRARY;

#endif