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

    dhcpv6def.h

Abstract:

    DHCPv6 Client Definitions

Author:

    FrancisD

Revision History:



--*/

#ifndef __DHCPV6_DEF_H__
#define __DHCPV6_DEF_H__


//
// Definitions for DHCPv6 Ports
//
#define DHCPV6_CLIENT_LISTEN_PORT  546
#define DHCPV6_SERVER_LISTEN_PORT  547


//
// Definitions for DHCPv6 Addresses
//
#define All_DHCP_Relay_Agents_and_Servers   L"FF02::1:2"


//
// Timeout Definitions (seconds)
//
#define DHCPV6_SOL_MAX_DELAY     1
#define DHCPV6_SOL_TIMEOUT       1
#define DHCPV6_SOL_MAX_RT      120
#define DHCPV6_REQ_TIMEOUT       1
#define DHCPV6_REQ_MAX_RT       30
#define DHCPV6_REQ_MAX_RC       10
#define DHCPV6_CNF_MAX_DELAY     1
#define DHCPV6_CNF_TIMEOUT       1
#define DHCPV6_CNF_MAX_RT        4
#define DHCPV6_CNF_MAX_RD       10
#define DHCPV6_REN_TIMEOUT      10
#define DHCPV6_REN_MAX_RT      600
#define DHCPV6_REB_TIMEOUT      10
#define DHCPV6_REB_MAX_RT      600
#define DHCPV6_INF_MAX_DELAY     1
#define DHCPV6_INF_TIMEOUT       1
#define DHCPV6_INF_MAX_RT      120
#define DHCPV6_REL_TIMEOUT       1
#define DHCPV6_REL_MAX_RC        5
#define DHCPV6_DEC_TIMEOUT       1
#define DHCPV6_DEC_MAX_RC        5
#define DHCPV6_REC_TIMEOUT       2
#define DHCPV6_REC_MAX_RC        8
#define DHCPV6_HOP_COUNT_LIMIT  32


#endif // __DHCPV6_DEF_H__




















