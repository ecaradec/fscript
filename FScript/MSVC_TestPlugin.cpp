// MSVC_TestPlugin.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "MSVC_TestPlugin.h"

//-----------------------------------------------------------------------
// Plugin-specific file
#include "JrPlugin_MyPlugin.h"
//-----------------------------------------------------------------------

BOOL WINAPI DllMain(HINSTANCE hinstDLL,DWORD fdwReason,LPVOID lpvReserved)
{
    return TRUE;
}
