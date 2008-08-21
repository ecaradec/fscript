// JrPlugin_MyPlugin
// VS wants this
#include "stdafx.h"
// System includes
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <atlbase.h>
#include <atlcom.h>

#include <atlstr.h>
#include <vector>
#include <map>

#include "fscript_h.h"
#include "fscript_i.c"

// our partner header which includes the funcs we need in the right way
#include "JrPlugin_MyPlugin.h"



// see JrPluginFuncs_FARR.h for function definitions we will be implementing here
// global state info
E_SearchStateT current_searchstate=E_SearchState_Stopped;
BOOL current_lockstate=FALSE;

struct Result
{
    Result() {}
    Result(const CString &title_, const CString &path_, const CString &icon_, E_EntryTypeT entrytype_=E_EntryType_FILE, E_ResultPostProcessingT matching_=E_ResultPostProcessing_MatchAgainstSearch, int score_=300)
        :score(score_), title(title_), path(path_), icon(icon_), entrytype(entrytype_), matching(matching_) {
    }
    int                 score;
    CString             title;
    CString             path;
    CString             icon;
    E_EntryTypeT        entrytype;
    E_ResultPostProcessingT matching;
};

std::vector<Result> tmpResultsA;
std::vector<Result> tmpResultsB;
std::vector<Result> *results=&tmpResultsA;
int g_queryKey=0;

//std::map<CString, Result> refresults;
//
// farr-specific function pointer so we can call to inform the host when we have results
Fp_GlobalPluginCallback_NotifySearchStateChanged callbackfp_notifysearchstatechanged=NULL;

// These functions are our plugin-specific versions of the generic
//  functions exported by the generic DLL shell.
// See JrPlugin_GenericShell.h and .cpp for descriptions of what these
//  functions are supposed to do.
BOOL MyPlugin_DllEntryPoint(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        break;

    case DLL_PROCESS_DETACH:
        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        break;
    }
    // success
    return TRUE;
}
#include <stdio.h>
#import "msscript.ocx" raw_interfaces_only // msscript.ocx 
using namespace MSScriptControl;

IScriptControlPtr pScriptControl;

struct FarrAtlModule : CAtlModule {
    HRESULT AddCommonRGSReplacements(IRegistrarBase *) {
        return S_OK;
    }
} _atlmodule;

struct FarrObject : CComObjectRoot,  
                    IDispatchImpl<IFARR, &__uuidof(IFARR), &LIBID_FARRLib, 0xFFFF, 0xFFFF>
{    
    BEGIN_COM_MAP(FarrObject)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    STDMETHOD(emitResult)(UINT querykey, BSTR title, BSTR path, BSTR icon, int entrytype, int resultpostprocessing, int score)
    {
        // ignore previous query
        if(querykey!=g_queryKey)
            return S_FALSE;
        results->push_back(Result(CString(title), CString(path), CString(icon), E_EntryTypeT(entrytype), E_ResultPostProcessingT(resultpostprocessing), score));
        return S_OK;
    }
    STDMETHOD(setState)(UINT querykey, UINT s)
    {
        // ignore previous query
        if(querykey!=g_queryKey)
            return S_FALSE;

        current_searchstate=(E_SearchStateT)s;
        ExecuteCallback_SearchStateChanged();
        return S_OK;
    }
    STDMETHOD(notifyStateChange)(UINT querykey)
    {
        // ignore previous query
        if(querykey!=g_queryKey)
            return S_FALSE;

        ExecuteCallback_SearchStateChanged();
        return S_OK;
    }
    STDMETHOD(setStrValue)(BSTR command, BSTR value)
    {
        callbackfp_set_strval(hostptr, CString(command), (char*)(const char*)CString(value));
        return S_OK;
    }
    STDMETHOD(debug)(BSTR txt)
    {
        MessageBox(0, CString(txt), "", MB_OK);
        return S_OK;
    }
};

CString currentDirectory;

void InitializeScriptControl()
{    
    pScriptControl.CreateInstance(__uuidof(ScriptControl));    
	pScriptControl->put_AllowUI(VARIANT_TRUE);
    
    CString language="";
    FILE *f=0;  
    f=fopen(currentDirectory+"\\fscript.js", "rb");
    language="JScript";

    /*if(f=fopen(currentDirectory+"\\fscript.js", "rb")) {
        language="JScript";
    } else if(f=fopen(currentDirectory+"\\fscript.vbs", "rb")) {
        language="VBScript";
    } else if(f=fopen(currentDirectory+"\\fscript.rb", "rb")) {
        language="RubyScript";
    } else if(f=fopen(currentDirectory+"\\fscript.py", "rb")) {
        language="Python";
    } else if(f=fopen(currentDirectory+"\\fscript.pl", "rb")) {
        language="PerlScript";
    }*/

    if(!f) {
        MessageBox(0, "Can't open file "+currentDirectory+"\\fscript.js.", "FScript error", MB_OK);
        return;
    }

    int l=_filelength(f->_file);
    char *content=(char*)malloc(l+1);
    fread(content, l, 1, f);
    content[l]=0;
    fclose(f);

    HRESULT hr=pScriptControl->put_Language(_bstr_t(language));

    CComObject<FarrObject> *pFarrObject=0;
    CComObject<FarrObject>::CreateInstance(&pFarrObject);
    pFarrObject->AddRef();
    
    hr=pScriptControl->AddObject(_bstr_t("FARR"), pFarrObject, VARIANT_FALSE);
    hr=pScriptControl->AddCode(_bstr_t(content));
    free(content);
    CComPtr<IScriptError> pE;
    pScriptControl->get_Error(&pE);
    if(pE) {
        CComBSTR descBSTR;
        pE->get_Description(&descBSTR);

        long line;
        pE->get_Line(&line);
        CString desc(descBSTR);

        if(desc!="") {
            MessageBox(0, "Script error in "+currentDirectory+"\\fscript.js : "+CString(desc), "FScript error", MB_OK);
            return;
        }
    }
}

BOOL MyPlugin_DoInit()
{
    FarrAtlModule::m_libid=LIBID_FARRLib;
    HRESULT hr = CoInitialize(NULL);
    GetCurrentDirectory(MAX_PATH, currentDirectory.GetBufferSetLength(MAX_PATH));
    currentDirectory.ReleaseBuffer();
    InitializeScriptControl();
    CComVariant ret;
    CComPtr<IDispatch> pCO;
    pScriptControl->get_CodeObject(&pCO);
    if(pCO)
        pCO.Invoke1(L"onInit", &CComVariant(currentDirectory), &ret);
    return TRUE;
}
BOOL MyPlugin_DoShutdown()
{
    // success
    pScriptControl->Reset();
    pScriptControl=0;
    CoUninitialize();
    return TRUE;
}
BOOL MyPlugin_GetStrVal(const char* varname,char *destbuf, int maxlen)
{    
    CComVariant v;
    try
    {	
        pScriptControl->Eval(CComBSTR(varname),&v);
        strcpy(destbuf,CString(v));
        return TRUE;
    } catch(...) {
        if(currentDirectory!="")
            MessageBox(0, "FARR variable '"+CString(varname)+"' was not returned for "+currentDirectory+"\\fscript.js", "FScript error", MB_OK);
        strcpy(destbuf,CString("FScript"));
        return TRUE;
    }
    // not found
    return FALSE;
}
BOOL MyPlugin_SetStrVal(const char* varname, void *val)
{
    // farr host will pass us function pointer we will call
    if (strcmp(varname,DEF_FieldName_NotifySearchCallbackFp)==0)
        callbackfp_notifysearchstatechanged = (Fp_GlobalPluginCallback_NotifySearchStateChanged)val;

    return FALSE;
}
BOOL MyPlugin_SupportCheck(const char* testname, int version)
{
    // ATTN: we support farr interface
    if (strcmp(testname,DEF_FARRAPI_IDENTIFIER)==0)
        return TRUE;

    // otherwise we don't support it
    return FALSE;
}
BOOL MyPlugin_DoAdvConfig()
{
    // success
    ShellExecuteA(NULL, "edit", currentDirectory+"\\fscript.js", NULL,NULL, SW_SHOWNORMAL);
    return TRUE;
}
BOOL MyPlugin_DoShowReadMe()
{
    // by default show the configured readme file
    char fname[MAX_PATH+1];
    strcpy(fname, dlldir);
    strcat(fname, "\\");
    strcat(fname, ThisPlugin_ReadMeFile);
    ShellExecuteA(NULL, "open", fname, NULL,NULL, SW_SHOWNORMAL);

    // success
    return TRUE;
}
BOOL MyPlugin_SetState(int newstateval)
{
    // usually there is nothing to do here

    // success
    return TRUE;
}


// Host is asking us if we want a certain feature or call
// We should use a switch function here to tell what is being asked
//
// Returns TRUE or FALSE depending on whether we want the feature
//
PREFUNCDEF BOOL EFuncName_Ask_WantFeature(E_WantFeaturesT featureid)
{
    //return featureid==E_WantFeatures_searchinput_regex;
    switch (featureid)
    {
    case E_WantFeatures_searchinput_regex:
        // do we want to search on regular expression matches
        return TRUE;
    case E_SupportFeatures_searchinput_explicit:
        // do we want to search on alias match
        return TRUE;
    case E_SupportFeatures_searchinput_all:
        // do we want to search on all search expressions
        return TRUE;
    case E_SupportFeatures_tryhandle_trigger:
        // do we want to try to handle triggers when a result is launched
        return TRUE;
    case E_SupportFeatures_addinfo_files:
        // do we want to try to handle triggers when a result is launched
        return FALSE;
    case E_SupportFeatures_scoreall_files:
        // do we want to try to score EVERY file considered (SLOWS DOWN SEARCH!)
        return FALSE;
    case E_SupportFeatures_scoregood_files:
        // do we want to try to score files after other scoring has determined they are a viable result
        return FALSE;
    case E_SupportFeatures_dominate_results:
        // do we want our results to dominate and hide any others
        return FALSE;
    }

    // fell through - so not supported
    return FALSE;
}

// These functions are FARR-SPECIFIC and have no counterparts in the
//  generic DLL shell.
//
// Note from the function declarations that these functions are EXPORTED
//  from the DLL so that the host can call them.

// Host informs us that a search is begining and what the searchstring is
// The Plugin DLL could do all "searching" here (if there is indeed any to do)
// And compute all results within this function, OR simply begin a threaded asynchronous search now.
//
// Normally the string you care about is searchstring_lc_nokeywords, thats the search string with all special +modifier keywords removed, and
//  the alias keyword for this plugin removed, so all thats left is the string that should effect the results of the search.
//
// returns TRUE only if the plugin decides now that no more searching should be done by any other plugin or builtin
//
// NOTE: if asynchronous searching is to be done, make sure to only set current_searchstate=E_SearchState_Stopped when done!
//


PREFUNCDEF BOOL EFuncName_Inform_SearchBegins(const char* searchstring_raw, const char *searchstring_lc_nokeywords, BOOL explicitinvocation)
{
    CComVariant ret;

    //refresults.clear();
    results->clear();
    CComPtr<IDispatch> pCO;
    pScriptControl->get_CodeObject(&pCO);
    CComVariant varArgs[] = { CComVariant(searchstring_lc_nokeywords), CComVariant(searchstring_raw), CComVariant(BOOL(explicitinvocation)), CComVariant(UINT(g_queryKey)) }; 
    HRESULT hr=pCO.InvokeN(L"onSearchBegin", varArgs, _countof(varArgs), &ret);    

    return ret.vt==VT_BOOL && ret.boolVal==VARIANT_TRUE;
} 


// Host informs us that regular expression match **HAS** occured, and to begin doing that we need to do on such a match
// The Plugin DLL could do all "searching" here (if there is indeed any to do)
// And compute all results within this function, OR simply begin a threaded asynchronous search now.
//
// To access the group capture strings, use regexcharps[1] to match the first group, etc.
//
// returns TRUE only if the plugin decides now that no more searching should be done by any other plugin or builtin
//
// NOTE: if asynchronous searching is to be done, make sure to only set current_searchstate=E_SearchState_Stopped when done!
//
PREFUNCDEF BOOL EFuncName_Inform_RegexSearchMatch(const char* searchstring_raw, const char *searchstring_lc_nokeywords, int regexgroups, char** regexcharps)
{
    CComVariant ret;

    //refresults.clear();
    results->clear();
    CComPtr<IDispatch> pCO;
    pScriptControl->get_CodeObject(&pCO);
    CComVariant varArgs[] = { CComVariant(searchstring_lc_nokeywords), CComVariant(searchstring_raw), CComVariant(UINT(g_queryKey)) }; 
    HRESULT hr=pCO.InvokeN(L"onRegexSearchMatch", varArgs, _countof(varArgs), &ret);    

    return ret.vt==VT_BOOL && ret.boolVal==VARIANT_TRUE;
}


// Host is requesting one of our results
// Fill the destination buffers without exceding maxlen
//
// resultpostprocmodep is from enum E_ResultPostProcessingT { E_ResultPostProcessing_ImmediateDisplay = 0, E_ResultPostProcessing_AddScore = 1, E_ResultPostProcessing_MatchAgainstSearch = 2};
//
// Returns TRUE on success
//
PREFUNCDEF BOOL EFuncName_Request_ItemResultByIndex(int resultindex, char *destbuf_path, char *destbuf_caption, char *destbuf_groupname, char *destbuf_iconfilename, void** tagvoidpp, int maxlen, E_ResultPostProcessingT *resultpostprocmodep, int *scorep, E_EntryTypeT *entrytypep)
{
    strcpy(destbuf_groupname,"");
    *tagvoidpp=NULL;    
    
    if(resultindex>results->size())
        return false;

    Result &r((*results)[resultindex]);
    strncpy(destbuf_caption, r.title.GetBuffer(), maxlen);
    strncpy(destbuf_path, r.path.GetBuffer(), maxlen);
    if(r.icon!="")
        strncpy(destbuf_iconfilename, (currentDirectory+"\\"+r.icon).GetBuffer(), maxlen);
    else
        strncpy(destbuf_iconfilename, "", maxlen);

    *entrytypep=r.entrytype;
    *resultpostprocmodep=r.matching;
    *scorep=r.score;    

    // ok filled one
    return TRUE;
}


// Host informs us that a search has completed or been interrupted, and we should stop any processing we might be doing assynchronously
//
// NOTE: make sure to set current_searchstate=E_SearchState_Stopped;
//
PREFUNCDEF void EFuncName_Inform_SearchEnds()
{
    g_queryKey++;

    // stop search and set this
    current_searchstate=E_SearchState_Stopped;

    // ATTN: test clear results
    //refresults.clear();
    results->clear();

    // notify host that our state has changed
    ExecuteCallback_SearchStateChanged();    

    return;
 }


// Host wants to know what kind if any results we have available
//
// NOTE: return is from (see JrPluginFuncs_FARR.h) enum E_ResultAvailableStateT { E_ResultAvailableState_None=0, E_ResultAvailableState_ItemResuls=1, E_ResultAvailableState_DisplayText=2, E_ResultAvailableState_DisplayHtml=3, E_ResultAvailableState_Grid=4, E_ResultAvailableState_WinTakeover=5 };
//
PREFUNCDEF E_ResultAvailableStateT EFuncName_Ask_IsAnyResultAvailable()
{
    // this will be tracked elsewhere    
    return results->size()?E_ResultAvailableState_ItemResuls:E_ResultAvailableState_None;
}

// Host wants to know how many item results are available
PREFUNCDEF int EFuncName_Ask_HowManyItemResultsAreReady()
{
    // this will be tracked elsewhere
    return results->size();
}

// Host calls this before iterating through the result requests
// It will be TRUE on lock before retrieving
//  and then FALSE when all done retrieving
// In this way the dll should refrain from altering the results until retrieval is done
// One way to do this for asynchronous searching is to MOVE results to a return store
//  on TRUE lock, and then free on FALSE.
//
// Returns TRUE on success
//
PREFUNCDEF BOOL EFuncName_Request_LockResults(BOOL dolock)
{
    // set lock
    current_lockstate=dolock;

    if (dolock==false)
    {
        //for(std::vector<Result>::iterator it=results->begin(); it!=results->end(); it++)
        //    refresults[it->title]=*it;

        if(results==&tmpResultsA)
            results=&tmpResultsB;
        else
            results=&tmpResultsA;
        
        // on unlocking they have retrieved all the results, so CLEAR the results or they will be found again
        results->clear();
        // notify host that our state has changed
        ExecuteCallback_SearchStateChanged();        
    }

    // success
    return TRUE;
}
// Host informs us that our window is about to appear
PREFUNCDEF void EFuncName_Inform_WindowIsHiding(HWND hwndp)
{
    return;
}

// Host informs us that our window is about to disappear
PREFUNCDEF void EFuncName_Inform_WindowIsUnHiding(HWND hwndp)
{
    return;
}


// Host wants to know the search state of the plugin
// returns:  E_SearchState_Stopped=0 || E_SearchState_Searching=1
//
// NOTE: this value will be based on a dll global variable
//
PREFUNCDEF E_SearchStateT EFuncName_Ask_SearchState()
{
    // this will be tracked elsewhere
    return current_searchstate;
}

// Host is asking us to return a char* to a LONG result string
//
// NOTE: It is up to us to free this char* when the lock is set to false above OR
//  on the next call, or on search end, but we MUST keep it valid until then.
PREFUNCDEF BOOL EFuncName_Request_TextResultCharp(char **charp)
{
    // signify we have none
    *charp=NULL;

    // none returned
    return FALSE;
}

// Host is calling us with a result item about to be triggered
// We can choose here to takeover the launching of it, OR simply do something and let normal launching continue
//
// Return TRUE to takeover launching and prevent all other further launching
// or FALSE to continue launching after we return
//
PREFUNCDEF BOOL EFuncName_Allow_ProcessTrigger(const char* destbuf_path, const char* destbuf_caption, const char* destbuf_groupname, int pluginid,int thispluginid, int score, E_EntryTypeT entrytype, void* tagvoidp, BOOL *closeafterp)
{
    // does this plugin want to take over launching of this result?
    if (thispluginid!=pluginid)
    {
        // if we didnt create it, let default handler do it
        return FALSE;
    }
    else
    {
        CComVariant ret;
        CComPtr<IDispatch> pCO;
        pScriptControl->get_CodeObject(&pCO);
        pCO.Invoke2(L"onProcessTrigger", &CComVariant(destbuf_path), &CComVariant(destbuf_caption), &ret);

        return ret.vt==VT_BOOL && ret.boolVal==VARIANT_TRUE;
    }

    return FALSE;
}

// Host is asking us if we want to modify the score of an item
// Original score in in *score, we can now modify it if we want
//
// Return TRUE if we modified it, or FALSE if not
//
// NOTE: set score to -9999 to eliminate it from results
//
PREFUNCDEF BOOL EFuncName_Do_AdjustResultScore(const char* itempath, int *scorep)
{
    // we didnt modify it
    return FALSE;
}

// Helper Functions the plugin calls
void ExecuteCallback_SearchStateChanged()
{
    // tell the host that our state or resultcount has changed
    if (callbackfp_notifysearchstatechanged)
    {
        callbackfp_notifysearchstatechanged(hostptr,results->size(),current_searchstate);
    }
}
