//-----------------------------------------------------------------------
// PluginFuncs_FARR.h
//-----------------------------------------------------------------------


//---------------------------------------------------------------------------
// Header Guard
#ifndef PluginFuncs_FARRH
#define PluginFuncs_FARRH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <windows.h>
//---------------------------------------------------------------------------


//-----------------------------------------------------------------------
// Inlcude generic plugin func definitions (shared with host)
#include "JrPluginFuncs.h"
//-----------------------------------------------------------------------




//---------------------------------------------------------------------------
// Now we add FARR-specific functions like those found in PluginFuncs.h
//---------------------------------------------------------------------------


//-----------------------------------------------------------------------
// defines used in host and dll
//
// for supportcheck stuff
#define DEF_FARRAPI_IDENTIFIER	"farrapi"
//-----------------------------------------------------------------------


//-----------------------------------------------------------------------
// Fields that should be handled in DLL EFuncName_GetStrVal()
#define DEF_FieldName_AliasStr		"aliasstr"
#define DEF_FieldName_RegexStr		"regexstr"
#define DEF_FieldName_RegexFilterStr	"regexfilterstr"
#define DEF_FieldName_KeywordStr	"keywordstr"
#define DEF_FieldName_ScoreStr		"scorestr"
#define DEF_FieldName_DefaultSearchText	"defaultsearchtext"
//
#define DEF_FieldName_NotifySearchCallbackFp	"notifysearchcallback"
#define DEF_FieldName_EnterInRichEdit "enterinrichedit"
//-----------------------------------------------------------------------


//-----------------------------------------------------------------------
#define DEF_GRABRESULTS_MAXLEN	512
//-----------------------------------------------------------------------


//---------------------------------------------------------------------------
// States used by PluginFunc_Ask_SearchState below
enum E_SearchStateT { E_SearchState_Stopped=0, E_SearchState_Searching=1  , E_SearchState_dummybig=79000};
enum E_ResultAvailableStateT { E_ResultAvailableState_None=0, E_ResultAvailableState_ItemResuls=1, E_ResultAvailableState_DisplayText=2, E_ResultAvailableState_DisplayHtml=3, E_ResultAvailableState_Grid=4, E_ResultAvailableState_WinTakeover=5 , E_ResultAvailableState_dummybig=79000};
enum E_WantFeaturesT { E_WantFeatures_searchinput_regex=0, E_SupportFeatures_searchinput_explicit=1, E_SupportFeatures_searchinput_all=2, E_SupportFeatures_tryhandle_trigger=3, E_SupportFeatures_addinfo_files=4, E_SupportFeatures_scoreall_files=5, E_SupportFeatures_scoregood_files=6, E_SupportFeatures_dominate_results=7 , E_SupportFeatures_dummybig=79000};
enum E_ResultPostProcessingT { E_ResultPostProcessing_ImmediateDisplay = 0, E_ResultPostProcessing_AddScore = 1, E_ResultPostProcessing_MatchAgainstSearch = 2 , E_ResultPostProcessing_AddScore_wPats =3 , E_ResultPostProcessing_MatchAgainstSearch_wPats = 4, E_ResultPostProcessing_dummybig=79000};
enum E_EntryTypeT { E_EntryType_UNKNOWN=0, E_EntryType_FILE=1, E_EntryType_FOLDER=2, E_EntryType_ALIAS=3, E_EntryType_URL=4  , E_EntryType_PLUGIN=5, E_EntryType_CLIP=5, E_EntryType_dummybig=79000};
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Plugin DLL should implement these functions which can be CALLED by host
// See descriptions of these functions in the .cpp file

#define EFuncName_Inform_SearchBegins PluginFunc_Inform_SearchBegins
typedef BOOL (*FpFunc_Inform_SearchBegins)(const char* searchstring_raw, const char *searchstring_lc_nokeywords, BOOL explicitinvocation);

#define EFuncName_Inform_RegexSearchMatch PluginFunc_Inform_RegexSearchMatch
typedef BOOL (*FpFunc_Inform_RegexSearchMatch)(const char* searchstring_raw, const char *searchstring_lc_nokeywords, int regexgroups, char** regexcharps);

#define EFuncName_Inform_SearchEnds PluginFunc_Inform_SearchEnds
typedef void (*FpFunc_Inform_SearchEnds)();

#define EFuncName_Inform_WindowIsHiding PluginFunc_Inform_WindowIsHiding
typedef void (*FpFunc_Inform_WindowIsHiding)(HWND hwndp);

#define EFuncName_Inform_WindowIsUnHiding PluginFunc_Inform_WindowIsUnHiding
typedef void (*FpFunc_Inform_WindowIsUnHiding)(HWND hwndp);

#define EFuncName_Ask_WantFeature PluginFunc_Ask_WantFeature
typedef BOOL (*FpFunc_Ask_WantFeature)(E_WantFeaturesT featureid);

#define EFuncName_Ask_SearchState PluginFunc_Ask_SearchState
typedef E_SearchStateT (*FpFunc_Ask_SearchState)();

#define EFuncName_Ask_IsAnyResultAvailable PluginFunc_Ask_IsAnyResultAvailable
typedef E_ResultAvailableStateT (*FpFunc_Ask_IsAnyResultAvailable)();

#define EFuncName_Ask_HowManyItemResultsAreReady PluginFunc_Ask_HowManyItemResultsAreReady
typedef int (*FpFunc_Ask_HowManyItemResultsAreReady)();

#define EFuncName_Request_LockResults PluginFunc_Request_LockResults
typedef BOOL (*FpFunc_Request_LockResults)(BOOL dolock);

#define EFuncName_Request_ItemResultByIndex PluginFunc_Request_ItemResultByIndex
typedef BOOL (*FpFunc_Request_ItemResultByIndex)(int resultindex, char *destbuf_path, char *destbuf_caption, char *destbuf_groupname, char *destbuf_iconfilename, void** tagvoidpp, int maxlen, E_ResultPostProcessingT *resultpostprocmodep, int *scorep, E_EntryTypeT *entrytypep);

#define EFuncName_Request_TextResultCharp PluginFunc_Request_TextResultCharp
typedef BOOL (*FpFunc_Request_TextResultCharp)(char **charp);

#define EFuncName_Allow_ProcessTrigger PluginFunc_Allow_ProcessTrigger
typedef BOOL (*FpFunc_Allow_ProcessTrigger)(const char* destbuf_path, const char* destbuf_caption, const char* destbuf_groupname, int pluginid,int thispluginid, int score, E_EntryTypeT entrytype, void* tagvoidp, BOOL *closeafterp);

#define EFuncName_Do_AdjustResultScore PluginFunc_Do_AdjustResultScore
typedef BOOL (*FpFunc_Do_AdjustResultScore)(const char* itempath, int *scorep);

#define EFuncName_Do_AddInfo PluginFunc_Do_AddInfo
typedef BOOL (*FpFunc_Do_AddInfo)(const char* itempath);

#define EFuncName_ReceiveKey PluginFunc_ReceiveKey
typedef BOOL (*FpFunc_ReceiveKey)(int Key, int altpressed, int controlpressed, int shiftpressed);

#define EFuncName_IdleTime PluginFunc_IdleTime
typedef BOOL (*FpFunc_IdleTime)(int elapsedmilliseconds);
//---------------------------------------------------------------------------


//-----------------------------------------------------------------------
// CALLBACKS
// implemented in PluginManager (not in your dll!)
#define EFuncName_GlobalPluginCallback_NotifySearchStateChanged	PluginHostFunc_GlobalPluginCallback_NotifySearchStateChanged
typedef void (*Fp_GlobalPluginCallback_NotifySearchStateChanged)(void *JrPluginPointerp, int newresultcount,E_SearchStateT newstate);
PREFUNCDEFCB void EFuncName_GlobalPluginCallback_NotifySearchStateChanged(void *JrPluginPointerp, int newresultcount,E_SearchStateT newstate);
//-----------------------------------------------------------------------














//-----------------------------------------------------------------------
#endif
//-----------------------------------------------------------------------
