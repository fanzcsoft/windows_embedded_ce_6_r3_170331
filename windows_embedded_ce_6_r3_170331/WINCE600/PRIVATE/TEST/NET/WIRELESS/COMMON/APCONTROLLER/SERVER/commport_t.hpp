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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Definitions and declarations for the CommPort_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_CommPort_t_
#define _DEFINED_CommPort_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>
#include <stdio.h>
#include <inc/string.hxx>
#include <inc/sync.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a contention-controlled interface to one of the comm ports.
//
class CommHandle_t;
class CommPort_t
{
private:

    // Current comm-port connection:
    CommHandle_t *m_pHandle;

    // Copy and assignment are deliberately disabled:
    CommPort_t(const CommPort_t &src);
    CommPort_t &operator = (const CommPort_t &src);

public:
    
    // Contructor and destructor:
    CommPort_t(int PortNumber);
    virtual
   ~CommPort_t(void);

    // Retrieves an object which can be locked to prevent other threads
    // from using the port:
    // Callers should lock this object before performing any I/O operations.
    ce::critical_section &
    GetLocker(void);

    // Connects to the port and configures it for serial communication:
    DWORD
    Open(
        int BaudRate,
        int ByteSize,
        int Parity,
        int StopBits);

    // Closes the port connection:
    DWORD
    Close(void);

    // Determines whether the comm-port is open and configured:
    bool
    IsOpened(void) const;
    
    // Reads ASCII from the comm port and inserts it into the specified
    // ASCII or (after translation) Unicode buffer:
    DWORD
    Read(
        ce::string *pBuffer,
        int         MaxCharsToRead,
        int         MaxMillisToWait);
    DWORD
    Read(
        ce::wstring *pBuffer,
        int          MaxCharsToRead,
        int          MaxMillisToWait);

    // If necessary, converts the specified string to ASCII and writes
    // it to the port:
    DWORD
    Write(
        const ce::string &Message,
        int               MaxMillisToWait);
    DWORD
    Write(
        const ce::wstring &Message,
        int                MaxMillisToWait);
};

};
};
#endif /* _DEFINED_CommPort_t_ */
// ----------------------------------------------------------------------------
