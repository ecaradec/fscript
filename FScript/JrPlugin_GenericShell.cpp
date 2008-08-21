//-----------------------------------------------------------------------
// JrPlugin_GenericShell
//-----------------------------------------------------------------------


//-----------------------------------------------------------------------
// VS wants this
#include "stdafx.h"
//-----------------------------------------------------------------------


//-----------------------------------------------------------------------
// Our partner header which includes the funcs we need in the right way
#include "JrPlugin_GenericShell.h"
//-----------------------------------------------------------------------


//-----------------------------------------------------------------------
// IMOPRTANT:
// Header which includes the funcs we need for our specific type of plugin
// NOTE: This is the file you should create for your plugin. Each plugin
//  will have a different JrPlugin_MyPlugin.h and JrPlugin_MyPlugin.cpp
#include "JrPlugin_MyPlugin.h"
//-----------------------------------------------------------------------







//-----------------------------------------------------------------------
// global callback data we need to remember when we are initialized
Fp_GlobalPluginCallback_GetStrVal callbackfp_get_strval=NULL;
Fp_GlobalPluginCallback_SetStrVal callbackfp_set_strval=NULL;
//
void* hostptr=NULL;
bool wasanerror=false;
//
char dlldir[MAX_PATH];
char iconfpath[MAX_PATH];
//-----------------------------------------------------------------------























//---------------------------------------------------------------------------
// Initialization and Shutdown
//---------------------------------------------------------------------------





//-----------------------------------------------------------------------
// This function is called by the host program when it discovers plugins
//  It tells the plugin:
//    fullfilename - The dll's own filename
//    thisptr - A pointer it will use when performing callbacks
//    hostname - The name of the host program
//    hostversionstring - The char* string describing the host program version number
//    incallbackfp - pointer to global generic callback function of host
//    incallbackfp_get_strval - pointer to simple get_strval callback function of host
//
//    Most of the time all you have to do is record data you want to remember for later
//    Returns: FALSE if the plugin wants to refuse to be loaded (useful if we check
//     hostname/version and know we don't support it)
//
// NOTE: this would be a good place to call one-time allocation and object creation functions
//

PREFUNCDEF BOOL PluginFunc_DoInit(const char* fullfilename, void* thisptr, const char *hostname, const char *hostversionstring, Fp_GlobalPluginCallback_GetStrVal incallbackfp_get_strval, Fp_GlobalPluginCallback_SetStrVal incallbackfp_set_strval)
{
	// save values
	hostptr = thisptr;
	callbackfp_get_strval=incallbackfp_get_strval;
	callbackfp_set_strval=incallbackfp_set_strval;

	// save dir and icon for us
	strncpy(dlldir,fullfilename,MAX_PATH);
	char* rpathchar=strrchr(dlldir,'\\');
	if (rpathchar!=NULL)
		*(rpathchar+1)='\0';
	// icon absolute path
	strcpy(iconfpath,dlldir);

	// Program-Specific Initialization
	return MyPlugin_DoInit();
}
//-----------------------------------------------------------------------




//-----------------------------------------------------------------------
// This function is called by the host when the Plugin DLL is about to be unloaded.
//  You cannot refuse it.
//  Returns FALSE on error.
//
// NOTE: You might release any resources you allocated previously here, or
//  if you use visual forms, now would be the place to delete them.

PREFUNCDEF BOOL PluginFunc_DoShutDown()
{
	// Program-Specific Initialization
	return MyPlugin_DoShutdown();
}
//-----------------------------------------------------------------------




















//---------------------------------------------------------------------------
// Simple Generic Accessor
//---------------------------------------------------------------------------




//-----------------------------------------------------------------------
// This is a simple function to request some information returned as a
//  string (char*) stored in hosts memory space.
// It's meant to be super generic and very straightforward.
// Host passes the name of the variable/field corresponding to the desired data.
// DLL Plugin should fill in destbuf, up to maxlen characters.
// Standard maxlen sizes for standard fields is 1024 so its safe to use
//  this much space without checking maxlen.
// Returns FALSE if the varname field is not known.
//
// NOTE: make sure your plugin returns values for common fields.
//
PREFUNCDEF BOOL PluginFunc_GetStrVal(const char* varname,char *destbuf, int maxlen)
{
	// Some standard fields defined by #defines at top of file
	// So you shouldn't really have to mess with this stuff
	/*if (strcmp(varname,DEF_FieldName_DisplayName)==0)
		{
		strcpy(destbuf,ThisPlugin_DisplayName);
		return TRUE;
		}
	if (strcmp(varname,DEF_FieldName_VersionString)==0)
		{
		strcpy(destbuf,ThisPlugin_VersionString);
		return TRUE;
		}
	if (strcmp(varname,DEF_FieldName_ReleaseDateString)==0)
		{
		strcpy(destbuf,ThisPlugin_ReleaseDateString);
		return TRUE;
		}
	if (strcmp(varname,DEF_FieldName_Author)==0)
		{
		strcpy(destbuf,ThisPlugin_Author);
		return TRUE;
		}
	if (strcmp(varname,DEF_FieldName_UpdateURL)==0)
		{
		strcpy(destbuf,ThisPlugin_UpdateURL);
		return TRUE;
		}
	if (strcmp(varname,DEF_FieldName_HomepageURL)==0)
		{
		strcpy(destbuf,ThisPlugin_HomepageURL);
		return TRUE;
		}
	if (strcmp(varname,DEF_FieldName_ShortDescription)==0)
		{
		strcpy(destbuf,ThisPlugin_ShortDescription);
		return TRUE;
		}
	if (strcmp(varname,DEF_FieldName_LongDescription)==0)
		{
		strcpy(destbuf,ThisPlugin_LongDescription);
		return TRUE;
		}

	// Do we support advanced options? Return "" if not
	if (strcmp(varname,DEF_FieldName_AdvConfigString)==0)
		{
		strcpy(destbuf,ThisPlugin_AdvConfigString);
		return TRUE;
		}
	// Do we support a readme? Return "" if not
	if (strcmp(varname,DEF_FieldName_ReadMeString)==0)
		{
		strcpy(destbuf,ThisPlugin_ReadMeString);
		return TRUE;
		}

	// Icon filename for plugin?  We pass icons by icon filename
	if (strcmp(varname,DEF_FieldName_IconFilename)==0)
		{
		strcpy(destbuf,ThisPlugin_IconFilename);
		return TRUE;
		}*/

	// now defer to plugin specific
	return MyPlugin_GetStrVal(varname,destbuf,maxlen);
}
//-----------------------------------------------------------------------





//-----------------------------------------------------------------------
// This is a simple function which may allow the host to set certain options and values
// It may be unused by most plugins.
// Returns false if the variable is unknown or can't be set
PREFUNCDEF BOOL PluginFunc_SetStrVal(const char* varname, void *val)
{
	// plugin handles this
	return MyPlugin_SetStrVal(varname,val);
}
//-----------------------------------------------------------------------












//---------------------------------------------------------------------------
// Configuration Stuff
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// This is a helper function which the host calls to ask the DLL plugin
//  if it supports certain protocols/features/hosts/etc
// Normally you might simply call it with the testname=hostname
//  and version = hostversion
// But you could also support various features independently which the
//  host can query.
// It's up to the host to decide what to do based on the responses.
// The host may reject the plugin completely and decide not use the plugin
//  depending on the reply.
// Return true if the feature is supported.
//
// NOTE: when you write plugins for a program think ahead to future updates
//  of the host program and plugin and how to properly negotiate btwn them
//  to take care of cases where versions are mismatched.

PREFUNCDEF BOOL PluginFunc_SupportCheck(const char* testname, int version)
{
	return MyPlugin_SupportCheck(testname,version);
}
//---------------------------------------------------------------------------





//-----------------------------------------------------------------------
// This function is called by the host when user pushes button to
//  access ADVANCED options of the plugin.
// So you might here launch a windows dialog with advanced options, etc.
// Returns false on error.
//
// NOTE: If your plugin supports advanced options, you have to return
//  a non "" value for ThisPlugin_AdvConfigString
// NOTE: It is your responsibility to save and load your advanced options
//  though i expect to add a few callback functions for saving simple
//  string and int values for you.

PREFUNCDEF BOOL PluginFunc_DoAdvConfig()
{
	// this is all handled by plugin specific code
	return MyPlugin_DoAdvConfig();
}
//-----------------------------------------------------------------------




//-----------------------------------------------------------------------
// This function is called by the host when user pushes button to
//  access the readme or help file options of the plugin.
// So you might here launch a text file or help file and let shell display
// Returns false on error.
//
// NOTE: If your plugin supports advanced options, you have to return
//  a non "" value for ThisPlugin_ReadMeString
//
PREFUNCDEF BOOL PluginFunc_DoShowReadMe()
{
	// this is all handled by plugin specific code
	return MyPlugin_DoShowReadMe();
}
//-----------------------------------------------------------------------




//-----------------------------------------------------------------------
// This function is called to inform the plugin about a change of its
//  "state".  Usually the plugin can completely ignore this - as the
//  host will handle ignoring it when it is disable, but the function
//  is provided in case the plugin author has some use for it (like
//  freeing memory when the plugin is disabled).
// Returns false on error.
//
// Predefined values are: DEF_PluginState_Disabled, DEF_PluginState_Enabled
//
// NOTE: the PluginFunc_SetState(DEF_PluginState_Enabled) will be called
//  before first use of the plugin.

PREFUNCDEF BOOL PluginFunc_SetState(int newstateval)
{
	// this is all handled by plugin specific code
	return MyPlugin_SetState(newstateval);
}
//-----------------------------------------------------------------------























//---------------------------------------------------------------------------
// Internal Helper Functions
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Helper function your plugin can call to report an error to the host, which will be logged
void ReportErrorToHost(char *estring)
{
	// report an error to the host - usually there is no need for more plugin-specific code
	callbackfp_set_strval(hostptr,"reporterror",estring);
}
//---------------------------------------------------------------------------





