//-----------------------------------------------------------------------
// JrPlugin_GenericShell
//-----------------------------------------------------------------------


//---------------------------------------------------------------------------
// Header Guard
#ifndef JrPluginGenericShellH
#define JrPluginGenericShellH
//---------------------------------------------------------------------------







//-----------------------------------------------------------------------
// Need this before we call PluginFuncs.h so it knows we are a DLL
#define JrPlugin_THISISDLL
//-----------------------------------------------------------------------
//
//-----------------------------------------------------------------------
// Inlcude plugin func definitions (shared with host)
#include "JrPluginFuncs.h"
//-----------------------------------------------------------------------














//-----------------------------------------------------------------------
// global callback data we need to remember when we are initialized
extern Fp_GlobalPluginCallback_GetStrVal callbackfp_get_strval;
extern Fp_GlobalPluginCallback_SetStrVal callbackfp_set_strval;
//
extern void* hostptr;
//
extern char dlldir[];
extern char iconfpath[];
//-----------------------------------------------------------------------




//---------------------------------------------------------------------------
// Helper functions

// Forward declaration for internal use
void ReportErrorToHost(char *estring);
//-----------------------------------------------------------------------











//-----------------------------------------------------------------------
#endif
//-----------------------------------------------------------------------
