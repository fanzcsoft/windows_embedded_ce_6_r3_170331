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
//+----------------------------------------------------------------------------
//
//
// File:    enwsdlse.cpp
//
// Contents:
//
//  implementation file
//
//        IEnumWSDLService  Interface implemenation
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "enwsdlse.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


BEGIN_INTERFACE_MAP(CEnumWSDLService)
    ADD_IUNKNOWN(CEnumWSDLService, IEnumWSDLService)
    ADD_INTERFACE(CEnumWSDLService, IEnumWSDLService)
END_INTERFACE_MAP(CEnumWSDLService)




/////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CEnumWSDLService::CEnumWSDLService()
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CEnumWSDLService::CEnumWSDLService()
{
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CEnumWSDLService::~CEnumWSDLService()
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CEnumWSDLService::~CEnumWSDLService()
{
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumWSDLService::Next(long celt, IWSDLService **ppWSDLService, long *pulFetched)
//
//  parameters:
//
//  description:
//
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumWSDLService::Next(long celt, IWSDLService **ppWSDLService, long *pulFetched)
{
    return(m_serviceList.Next(celt, ppWSDLService, pulFetched));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumWSDLService::Skip(long celt)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumWSDLService::Skip(long celt)
{
    return(m_serviceList.Skip(celt));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumWSDLService::Reset(void)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumWSDLService::Reset(void)
{
    return(m_serviceList.Reset());
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumWSDLService::Clone(IEnumWSDLService **ppenum)
//
//  parameters:
//
//  description:
//
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumWSDLService::Clone(IEnumWSDLService **ppenum)
{
    HRESULT hr = S_OK;
    CEnumWSDLService *pServices;
    pServices = new CSoapObject<CEnumWSDLService>(INITIAL_REFERENCE);
    CHK_BOOL(pServices, E_OUTOFMEMORY);
    CHK(pServices->Copy(this));
    pServices->Reset();
    *ppenum = pServices;
Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumWSDLService::Copy(CEnumWSDLService *pOrg)
//
//  parameters:
//
//  description:
//
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumWSDLService::Copy(CEnumWSDLService *pOrg)
{
    return (pOrg->m_serviceList.Clone(m_serviceList));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumWSDLService::Find(BSTR bstrServiceToFind, IWSDLService **ppIWSDLService)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumWSDLService::Find(BSTR bstrServiceToFind, IWSDLService **ppIWSDLService)
{
    return(m_serviceList.Find(bstrServiceToFind, ppIWSDLService));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumWSDLService::Add(IWSDLService *pWSDLService)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumWSDLService::Add(IWSDLService *pWSDLService)
{
    return(m_serviceList.Add(pWSDLService));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumWSDLService::Size(long *pulSize)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumWSDLService::Size(long *pulSize)
{
    return(m_serviceList.Size(pulSize));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
