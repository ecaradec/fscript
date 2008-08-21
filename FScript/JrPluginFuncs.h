//---------------------------------------------------------------------------
// JrPluginFuncs.h
// Common function prototypes and stuff used by BOTH dll plugins and the host
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#ifndef JrPluginFuncsH
#define JrPluginFuncsH
//---------------------------------------------------------------------------




//-----------------------------------------------------------------------
// we need to include windows.h to get the BOOL define
// if you want to compile for non-windows, then do:
//     #define BOOL bool
#include "windows.h"
//-----------------------------------------------------------------------




//-----------------------------------------------------------------------
// Export and Import modifiers depending on which source code includes this file

#ifdef JrPlugin_USEDLL
 #ifdef __cplusplus
	#define PREFUNCDEF extern "C" __declspec( dllimport )
	#define PREFUNCDEFCB extern "C" __declspec( dllexport )
 #else
	#define PREFUNCDEF __declspec( dllimport )
	#define PREFUNCDEFCB __declspec( dllexport )
 #endif
#endif

#ifdef JrPlugin_THISISDLL
 #ifdef __cplusplus
	#define PREFUNCDEF extern "C" __declspec( dllexport )
	#define PREFUNCDEFCB extern "C" __declspec( dllimport )
 #else
	#define PREFUNCDEF __declspec( dllexport )
	#define PREFUNCDEFCB __declspec( dllimport )
 #endif
#endif
//-----------------------------------------------------------------------




//-----------------------------------------------------------------------
// Predefined Enabled and Disabled States (see PluginFunc_SetState)
enum E_PluginStateT { E_PluginState_Disabled=0, E_PluginState_Enabled=1, E_PluginState_dummybig=79000 };
//-----------------------------------------------------------------------



//---------------------------------------------------------------------------
// Some standard size limits
#define DEF_StandardMaxStrLen		1024
//---------------------------------------------------------------------------



//-----------------------------------------------------------------------
// Generic plugin fields that should be handled in DLL EFuncName_GetStrVal()

#define DEF_FieldName_DisplayName         "displayname"
#define DEF_FieldName_VersionString       "versionstring"
#define DEF_FieldName_ReleaseDateString   "releasedatestring"
#define DEF_FieldName_Author              "author"
#define DEF_FieldName_HomepageURL         "homepageurl"
#define DEF_FieldName_UpdateURL           "updateurl"
#define DEF_FieldName_ShortDescription    "shortdescription"
#define DEF_FieldName_LongDescription     "longdescription"
#define DEF_FieldName_AdvConfigString     "advconfigstring"
#define DEF_FieldName_ReadMeString        "readmestring"
#define DEF_FieldName_IconFilename        "iconfilename"
//-----------------------------------------------------------------------




















//-----------------------------------------------------------------------
// NOTE: These DLL functions which use ENUMs are all set up to use 32bit ints as ENUMS
//  so if you use another language besides c++, you need to make sure you send and receive these as 32bit (4byte) integers
//-----------------------------------------------------------------------




//-----------------------------------------------------------------------
// Plugin DLL can all these functions which are defined in HOST

// Called by plugin to get a string value or invoke a function in host
#define EFuncName_GlobalPluginCallback_GetStrVal PluginHostFunc_GlobalPluginCallback_GetStrVal
typedef BOOL (*Fp_GlobalPluginCallback_GetStrVal)(void *JrPluginPointerp, const char* varname,char *destbuf, int maxlen);
// implemented in PluginManager host (not in your plugin dll)
PREFUNCDEFCB BOOL EFuncName_GlobalPluginCallback_GetStrVal(void *JrPluginPointerp, const char* varname,char *destbuf, int maxlen);
//
// Called by plugin to set a value in the host or invoke a function in host
#define EFuncName_GlobalPluginCallback_SetStrVal PluginHostFunc_GlobalPluginCallback_SetStrVal
typedef BOOL (*Fp_GlobalPluginCallback_SetStrVal)(void *JrPluginPointerp, const char* varname,char *strbuf);
// implemented in PluginManager host (not in your plugin dll)
PREFUNCDEFCB BOOL EFuncName_GlobalPluginCallback_SetStrVal(void *JrPluginPointerp, const char* varname,char *strbuf);
//
// Host asks plugin about its progress
#define EFuncName_GlobalPluginCallback_UpdateProgress PluginHostFunc_GlobalPluginCallback_UpdateProgress
typedef void (*Fp_GlobalPluginCallback_UpdateProgress)(void *JrPluginPointerp, int progresspercent);
// implemented in PluginManager host (not in your plugin dll)
PREFUNCDEFCB void EFuncName_GlobalPluginCallback_UpdateProgress(void *JrPluginPointerp,  int progresspercent);
//
//-----------------------------------------------------------------------



//---------------------------------------------------------------------------
// Plugin DLL should implement these functions which can be CALLED by host
//

#define EFuncName_DoInit PluginFunc_DoInit
typedef BOOL (*FpFunc_DoInit)(const char* fullfilename, void* inhostptr, const char *hostname, const char *hostversionstring, Fp_GlobalPluginCallback_GetStrVal incallbackfp_GetStrVal, Fp_GlobalPluginCallback_SetStrVal incallbackfp_SetStrVal);

#define EFuncName_DoShutDown PluginFunc_DoShutDown
typedef BOOL (*FpFunc_DoShutDown)();

//

#define EFuncName_SupportCheck PluginFunc_SupportCheck
typedef BOOL (*FpFunc_SupportCheck)(const char* testname, int version);

#define EFuncName_SetState PluginFunc_SetState
typedef BOOL (*FpFunc_SetState)(E_PluginStateT newstateval);

#define EFuncName_DoAdvConfig PluginFunc_DoAdvConfig
typedef BOOL (*FpFunc_DoAdvConfig)();

#define EFuncName_DoShowReadMe PluginFunc_DoShowReadMe
typedef BOOL (*FpFunc_DoShowReadMe)();

//

#define EFuncName_GetStrVal PluginFunc_GetStrVal
typedef BOOL (*FpFunc_GetStrVal)(const char* varname,char *destbuf, int maxlen);

#define EFuncName_SetStrVal PluginFunc_SetStrVal
typedef BOOL (*FpFunc_SetStrVal)(const char* varname,void *val);
//---------------------------------------------------------------------------













//---------------------------------------------------------------------------
#endif
//---------------------------------------------------------------------------
