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

    dhcpv6hdr.h

Abstract:

    DHCPv6 Client Header Format

Author:

    Francis Duong

Revision History:



--*/

#ifndef __DHCPV6_HDR_H__
#define __DHCPV6_HDR_H__

//
// Definitions for DHCPv6 packet headers
//

typedef enum {
    DHCPV6_MESSAGE_TYPE_SOLICIT = 1,
    DHCPV6_MESSAGE_TYPE_ADVERTISE = 2,
    DHCPV6_MESSAGE_TYPE_REQUEST = 3,
    DHCPV6_MESSAGE_TYPE_CONFIRM = 4,
    DHCPV6_MESSAGE_TYPE_RENEW = 5,
    DHCPV6_MESSAGE_TYPE_REBIND = 6,
    DHCPV6_MESSAGE_TYPE_REPLY = 7,
    DHCPV6_MESSAGE_TYPE_RELEASE = 8,
    DHCPV6_MESSAGE_TYPE_DECLINE = 9,
    DHCPV6_MESSAGE_TYPE_RECONFIGURE = 10,
    DHCPV6_MESSAGE_TYPE_INFORMATION_REQUEST = 11,
    DHCPV6_MESSAGE_TYPE_RELAY_FORW = 12,
    DHCPV6_MESSAGE_TYPE_RELAY_REPL = 13
} DHCPV6_MESSAGE_TYPE, * PDHCPV6_MESSAGE_TYPE;


typedef struct DHCPV6_MESSAGE_HEADER {
    UCHAR MessageType;
    UCHAR TransactionID[3];
} DHCPV6_MESSAGE_HEADER, * PDHCPV6_MESSAGE_HEADER;


typedef enum {
    DHCPV6_OPTION_TYPE_CLIENTID = 1,
    DHCPV6_OPTION_TYPE_SERVERID = 2,
    DHCPV6_OPTION_TYPE_IA_NA = 3,
    DHCPV6_OPTION_TYPE_IA_TA = 4,
    DHCPV6_OPTION_TYPE_IAADDR = 5,
    DHCPV6_OPTION_TYPE_ORO = 6,
    DHCPV6_OPTION_TYPE_PREFERENCE = 7,
    DHCPV6_OPTION_TYPE_ELAPSED_TIME = 8,
    DHCPV6_OPTION_TYPE_RELAY_MSG = 9,
    DHCPV6_OPTION_TYPE_AUTH = 11,
    DHCPV6_OPTION_TYPE_UNICAST = 12,
    DHCPV6_OPTION_TYPE_STATUS_CODE = 13,
    DHCPV6_OPTION_TYPE_RAPID_COMMIT = 14,
    DHCPV6_OPTION_TYPE_USER_CLASS = 15,
    DHCPV6_OPTION_TYPE_VENDOR_CLASS = 16,
    DHCPV6_OPTION_TYPE_VENDOR_OPTS = 17,
    DHCPV6_OPTION_TYPE_INTERFACE_ID = 18,
    DHCPV6_OPTION_TYPE_RECONF_MSG = 19,
    DHCPV6_OPTION_TYPE_RECONF_ACCEPT = 20,
#ifdef UNDER_CE
    DHCPV6_OPTION_TYPE_DNS_SERVERS = 23,
    DHCPV6_OPTION_TYPE_DOMAIN_LIST = 24,
    DHCPV6_OPTION_TYPE_IA_PD = 25,
    DHCPV6_OPTION_TYPE_IA_PREFIX = 26
#else
    DHCPV6_OPTION_TYPE_DNS = 25,
#endif
} DHCPV6_OPTION_TYPE, * PDHCPV6_OPTION_TYPE;

typedef struct DHCPV6_OPTION_HEADER {
    USHORT OptionCode;
    USHORT OptionLength;
} DHCPV6_OPTION_HEADER, * PDHCPV6_OPTION_HEADER;

#define IA_PREFIX_OPTION_LEN    25

typedef struct DHCPV6_IA_PREFIX_OPTION {
    USHORT  OptionCode;
    USHORT  OptionLen;
    DWORD   PreferredLifetime;
    DWORD   ValidLifetime;
    UCHAR   PrefixLength;
    UCHAR   IPv6Prefix[16];
    UCHAR   PrefixOptions[1];
} DHCPV6_IA_PREFIX_OPTION, * PDHCPV6_IA_PREFIX_OPTION;


#define IA_PD_OPTION_LEN    12

typedef struct DHCPV6_IA_PD_OPTION {
    USHORT OptionCode;
    USHORT OptionLength;
    DWORD IAID;
    DWORD T1;
    DWORD T2;
    union {
        DHCPV6_IA_PREFIX_OPTION Prefix[1];
        BYTE PrefixByteLoc[1];
    };
} DHCPV6_IA_PD_OPTION, *PDHCPV6_IA_PD_OPTION;


typedef enum {
    DHCPV6_STATUS_SUCCESS = 0,
    DHCPV6_STATUS_UNSPECFAIL,
    DHCPV6_STATUS_NOADDRSAVAIL,
    DHCPV6_STATUS_NOBINDING,
    DHCPV6_STATUS_NOTONLINK,
    DHCPV6_STATUS_USEMULTICAST,
    DHCPV6_STATUS_NOPREFIXAVAIL

} DHCPV6_STATUS_CODE, *PDHCPV6_STATUS_CODE;
        


#endif // __DHCPV6_HDR_H__
