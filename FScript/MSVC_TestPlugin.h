// MSVC_TestPlugin.h : main header file for the MSVC_TestPlugin DLL
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CMSVC_TestPluginApp
// See MSVC_TestPlugin.cpp for the implementation of this class
//

class CMSVC_TestPluginApp : public CWinApp
{
public:
	CMSVC_TestPluginApp();

// Overrides
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
