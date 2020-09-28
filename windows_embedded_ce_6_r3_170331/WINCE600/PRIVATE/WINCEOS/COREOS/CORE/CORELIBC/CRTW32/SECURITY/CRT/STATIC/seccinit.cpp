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
/***
*seccinit.c - initialize the global buffer overrun security cookie
*
*Purpose:
*       Define __security_init_cookie, which is called at startup to initialize
*       the global buffer overrun security cookie used by the /GS compile flag.
*
*******************************************************************************/

#include <corecrt.h>
#include <windows.h>

/*
 * The global security cookie.  This name is known to the compiler.
 * Initialize to a garbage non-zero value just in case we have a buffer overrun
 * in any code that gets run before __security_init_cookie() has a chance to
 * initialize the cookie to the final value.
 */
extern "C"
{
__declspec(selectany) DWORD_PTR __security_cookie = DEFAULT_SECURITY_COOKIE;
__declspec(selectany) DWORD_PTR __security_cookie_complement = ~(DEFAULT_SECURITY_COOKIE);
}

/*
 * A helper routine to save space in each module
 */
extern "C" DWORD_PTR __security_gen_cookie2(void);

/***
*__security_init_cookie() - init buffer overrun security cookie.
*
*Purpose:
*       Initialize the global buffer overrun security cookie which is used by
*       the /GS compile switch to detect overwrites to local array variables
*       the potentially corrupt the return address.  This routine is called
*       at EXE/DLL startup.
*
*Entry:
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

extern "C"
void
__cdecl
__security_init_cookie(
    void
    )
{
    DWORD_PTR cookie = __security_cookie;
#if !defined(ENCODED_GS_COOKIE)
    DWORD_PTR tmp;
#endif

    /*
     * Do nothing if the global cookie has already been initialized.
     */

    if (cookie != 0 && cookie != DEFAULT_SECURITY_COOKIE)
    {
        __security_cookie_complement = ~cookie;
        return;
    }

    cookie = __security_gen_cookie2();

#if !defined(ENCODED_GS_COOKIE)
    /*
     * Zero the upper 16-bits of the cookie to help mitigate an attack
     * that overwrites it by MBS or wchar_t string functions.  This
     * is enforced by __security_check_cookie.
     *
     * Note, this is not useful on platforms that encode the cookie on the
     * stack by, for example, XORing with the frame pointer.
     */
    tmp = cookie & (((DWORD_PTR)-1) >> 16);
    cookie = (cookie >> 16) ^ tmp;
#endif

    /*
     * Make sure the global cookie is never initialized to zero, since in that
     * case an overrun which sets the local cookie and return address to the
     * same value would go undetected.
     */
    if (cookie == 0) {
        cookie = (DWORD_PTR)(unsigned __int16)DEFAULT_SECURITY_COOKIE;
    }

    __security_cookie = cookie;
    __security_cookie_complement = ~cookie;
}

