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
#include "atlsafe.h"
#include <vector>
#include <map>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <Wspiapi.h>

#include "fscript_h.h"
#include "fscript_i.c"

#include "libffi/ffi.h"


#import "msxml4.dll"

// our partner header which includes the funcs we need in the right way
#include "JrPlugin_MyPlugin.h"

#import "msscript.ocx" raw_interfaces_only // msscript.ocx 
using namespace MSScriptControl;

// 4244 : disable UINT_PTR cast to LONG warning
// 4312 : conversion from 'int' to 'HMENU' of greater size
// 4018: '>' : signed/unsigned mismatch
// 4267: 'return' : conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable : 4244 4312 4018 4267)


struct Result
{
    Result() {}
    Result(const CString &title_, const CString &path_, const CString &groupname_, const CString &icon_, E_EntryTypeT entrytype_, E_ResultPostProcessingT matching_, int score_, const CComVariant &args_)
        :score(score_), title(title_), path(path_), groupname(groupname_), icon(icon_), entrytype(entrytype_), matching(matching_), args(args_) {
    }
    int                     score;
    CString                 title;
    CString                 path;
    CString                 groupname;
    CString                 icon;
    E_EntryTypeT            entrytype;
    E_ResultPostProcessingT matching;
    CComVariant             args;
};


// see JrPluginFuncs_FARR.h for function definitions we will be implementing here
// global state info
E_SearchStateT                        g_current_searchstate=E_SearchState_Stopped;
BOOL                                  g_current_lockstate=FALSE;
std::vector<Result>                   g_tmpResultsA;
std::vector<Result>                   g_tmpResultsB;
std::vector<Result>                  *g_results=&g_tmpResultsA;
int                                   g_queryKey=0;
HWND                                  g_msgHwnd=0;
CString                               g_currentDirectory;
std::map<UINT,CComPtr<IDispatch> >    g_pendingTimers;
WNDCLASS                              g_dlgclass;
HWND                                  g_optionDlgHwnd=0;
char                                  g_fscriptDlgClassName[256];
char                                  g_fscriptClassName[256];
CString                               g_pluginname;
HWND                                  g_comHwnd;

// farr-specific function pointer so we can call to inform the host when we have results
Fp_GlobalPluginCallback_NotifySearchStateChanged callbackfp_notifysearchstatechanged=NULL;

void ShowOptionDialog();


IScriptControlPtr pScriptControl;

struct FarrAtlModule : CAtlModule {
    HRESULT AddCommonRGSReplacements(IRegistrarBase *) {
        return S_OK;
    }
} _atlmodule;


LRESULT __stdcall FScriptTimerProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if(msg==WM_TIMER) {
        g_pendingTimers[wparam].Invoke0(DISPID(0));
    }
    return CallWindowProc(g_dlgclass.lpfnWndProc,hwnd,msg,wparam,lparam); 
}

CComPtr<IXMLDOMDocument> g_optionsDlgDoc;
LRESULT __stdcall FScriptDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if(msg==WM_COMMAND && wparam==0xFFFF) {

        CComPtr<IXMLDOMNodeList> pList;
        g_optionsDlgDoc->selectNodes(CComBSTR("options/*"), &pList);
            
        long len;
        pList->get_length(&len);
        for(int i=0;i<len;i++) {
            CComPtr<IXMLDOMNode> pNode;
            pList->get_item(i, &pNode);
            CComPtr<IXMLDOMNamedNodeMap> pAttMap;
            pNode->get_attributes(&pAttMap);

            CComPtr<IXMLDOMNode> pValue;
            pAttMap->getNamedItem(CComBSTR("value"), &pValue);
            TCHAR buff[4096];
            GetDlgItemText(hwnd, i, buff, 4096);
            pValue->put_text(CComBSTR(buff));
        }

        g_optionsDlgDoc->save(CComVariant(g_currentDirectory+"\\options.xml"));
        g_optionsDlgDoc=0;
        DestroyWindow(hwnd);

        CComVariant ret;
        CComSafeArray<VARIANT> ary(ULONG(0));
        HRESULT hr=pScriptControl->Run(CComBSTR(L"onOptionsChanged"), ary.GetSafeArrayPtr(), &ret);

        g_optionDlgHwnd=0;
    } else if(msg==WM_COMMAND && wparam==0xFFFE) {
        g_optionsDlgDoc=0;
        DestroyWindow(hwnd);
        g_optionDlgHwnd=0;
    }

    return CallWindowProc(g_dlgclass.lpfnWndProc,hwnd,msg,wparam,lparam); 
}

WNDCLASS g_editClass;
LRESULT __stdcall EditProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if(msg==WM_CHAR && wparam==VK_TAB) {
        HWND hdlg=GetAncestor(hwnd, GA_PARENT);
        HWND hNext=GetNextDlgTabItem(hdlg, hwnd, GetKeyState(VK_SHIFT)&0x8000);
        SetFocus(hNext);
        SendMessage(hNext, EM_SETSEL, 0, -1);
        return S_OK;
    }
    return CallWindowProc(g_editClass.lpfnWndProc, hwnd, msg, wparam, lparam);
}

WNDCLASS g_btnClass;
LRESULT __stdcall ButtonProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if(msg==WM_CHAR && wparam==VK_TAB) {
        HWND hdlg=GetAncestor(hwnd, GA_PARENT);
        HWND hNext=GetNextDlgTabItem(hdlg, hwnd, GetKeyState(VK_SHIFT)&0x8000);
        SetFocus(hNext);
        SendMessage(hNext, EM_SETSEL, 0, -1);
        return S_OK;
    }
    return CallWindowProc(g_btnClass.lpfnWndProc, hwnd, msg, wparam, lparam);
}

struct TCPSocketObject : CComObjectRoot,  
                         IDispatchImpl<ITCPSocket, &__uuidof(ITCPSocket), &LIBID_FARRLib, 0xFFFF, 0xFFFF>
{    
    BEGIN_COM_MAP(TCPSocketObject)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    STDMETHOD(read)(BSTR *data)
    {
        CString output;

        int iResult;
        char buff[256];
        do {
            iResult = recv(m_sock, buff, sizeof(buff), 0);
            if(iResult==SOCKET_ERROR) {
                return E_FAIL;
            }

            output.Append(buff,iResult);            
        } while(iResult==sizeof(buff));

        *data=output.AllocSysString();        
        return S_OK;
    }
    STDMETHOD(write)(BSTR value)
    {
        CString v(value);
        send(m_sock, v.GetBuffer(), v.GetLength(), 0 );
        return S_OK;
    }
    STDMETHOD(close)()
    {
        closesocket(m_sock);
        return S_OK;
    }

    SOCKET m_sock;
};

struct FarrObject : CComObjectRoot,  
                    IDispatchImpl<IFARR, &__uuidof(IFARR), &LIBID_FARRLib, 0xFFFF, 0xFFFF>
{    
    BEGIN_COM_MAP(FarrObject)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    STDMETHOD(emitResult)(UINT querykey, BSTR title, BSTR path, BSTR icon, int entrytype, int resultpostprocessing, int score, BSTR groupname, VARIANT args)
    {
        // ignore previous query
        if(querykey!=g_queryKey)
            return S_FALSE;
        g_results->push_back(Result(CString(title), CString(path), CString(groupname), CString(icon), E_EntryTypeT(entrytype), E_ResultPostProcessingT(resultpostprocessing), score, CComVariant(args)));
        return S_OK;
    }
    STDMETHOD(setState)(UINT querykey, UINT s)
    {
        // ignore previous query
        if(querykey!=g_queryKey)
            return S_FALSE;

        g_current_searchstate=(E_SearchStateT)s;
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
    STDMETHOD(getStrValue)(BSTR command, BSTR *output)
    {        
        CString tmp;
        callbackfp_get_strval(hostptr, CString(command), tmp.GetBufferSetLength(4096), 4096); tmp.ReleaseBuffer();
        *output = tmp.AllocSysString();
        return S_OK;
    }
    STDMETHOD(debug)(BSTR txt)
    {
        MessageBox(0, CString(txt), g_pluginname, MB_OK);
        return S_OK;
    }
    STDMETHOD(setInterval)(UINT id, UINT elapse, IDispatch *pFunc)
    {
        // window for handling timers    
        if(g_msgHwnd==0) {
            g_msgHwnd=CreateWindowEx(0, "STATIC", "FScriptTimer", 0, 0, 0, 0, 0, 0, 0, 0 ,0);
            SetWindowLong(g_msgHwnd, GWL_WNDPROC, (UINT_PTR)FScriptTimerProc);
        }

        g_pendingTimers[id]=pFunc;
        SetTimer(g_msgHwnd, id, elapse, 0);
        return S_OK;
    }
    STDMETHOD(killInterval)(UINT id)
    {
        KillTimer(g_msgHwnd, id);
        g_pendingTimers.erase(id);
        return S_OK;
    }
    STDMETHOD(showOptions)()
    {
        ShowOptionDialog();
        return S_OK;
    }
    STDMETHOD(exec)(BSTR file, BSTR parameters, BSTR curDir, VARIANT_BOOL *bSuccess)
    {
        *bSuccess = ShellExecute(0, "open", CString(file), CString(parameters), CString(curDir), SW_NORMAL)!=0?VARIANT_TRUE:VARIANT_FALSE;
        return S_OK;
    }
    STDMETHOD(getIniValue)(BSTR file, BSTR section, BSTR key, BSTR def, BSTR *out)
    {
        CString o;
        GetPrivateProfileString(CString(section), CString(key), CString(def), o.GetBufferSetLength(4096), 4096, CString(file)); o.ReleaseBuffer();
        *out=o.AllocSysString();
        return S_OK;
    }
    STDMETHOD(setIniValue)(BSTR file, BSTR section, BSTR key, BSTR value)
    {
        CString o;
        BOOL b=WritePrivateProfileString(CString(section), CString(key), CString(value), CString(file));
        return S_OK;
    }
    STDMETHOD(getKeyState)(UINT vk, SHORT *state)
    {
        *state=GetKeyState(vk);
        return S_OK;
    }
    static BOOL CALLBACK FindTEdit(HWND hwnd, LPARAM lParam)
    {
        CString clss;
        GetClassName(hwnd, clss.GetBufferSetLength(128), 128);
        if(clss=="TEdit")
        {
            *(HWND*)lParam=hwnd;
            return FALSE;
        }
        return TRUE;
    }
    STDMETHOD(getQueryString)(BSTR *query)
    {
        HWND mainForm;
        BOOL b=callbackfp_get_strval(hostptr, "Handle.MainForm", (char*)(void*)&mainForm, sizeof(mainForm));

        HWND tedit=0;
        EnumChildWindows(mainForm, FindTEdit, (LPARAM)&tedit);

        CString txt;
        GetWindowText(tedit, txt.GetBufferSetLength(GetWindowTextLength(tedit)+1),GetWindowTextLength(tedit)+1); txt.ReleaseBuffer();
        *query=txt.AllocSysString();
        return S_OK;
    }
    STDMETHOD(getObject)(BSTR q, IDispatch **p)
    {
        ::CoGetObject(q, NULL, IID_IDispatch, (void**)p);
        return S_OK;
    }
    ffi_type *getFFITypeOfId(int i)
    {
        switch(i) {
            case 0: return &ffi_type_void; break;
            case 1: return &ffi_type_sint32; break;
            case 2: return &ffi_type_float; break;
            case 3: return &ffi_type_double; break;
            case 4: return &ffi_type_longdouble; break;
            case 5: return &ffi_type_uint8; break;
            case 6: return &ffi_type_sint8; break;
            case 7: return &ffi_type_uint32; break;
            case 8: return &ffi_type_sint16; break;
            case 9: return &ffi_type_uint32; break;
            case 10: return &ffi_type_sint32; break;
            case 11: return &ffi_type_uint64; break;
            case 12: return &ffi_type_sint64; break;
            case 13: break;                                 // each structure type should have its own id and return a structure type of correct length
            case 14: return &ffi_type_pointer; break;
            //case 15: return &ffi_type_pointer; break;   // char*
            //case 16: return &ffi_type_pointer; break;   // wchar*
        }
        return 0;
    }

    STDMETHOD(newCFunction)(BSTR absFnName_, int retValType_, SAFEARRAY **argsTypes_, VARIANT *pCFunc_)
    {
        CComSafeArray<VARIANT> argsTypes__(*argsTypes_);
        ffi_cif   cif;        
        ffi_type *retValType = getFFITypeOfId(retValType_);
        ffi_type **argsTypes = (ffi_type**)malloc(sizeof(ffi_type*)*(*argsTypes_)->cbElements);
        for(int i=0; i<argsTypes__.GetCount(); i++) {
            
            if(argsTypes__.GetAt(i).vt!=VT_I4)
                return S_FALSE;
            argsTypes[i]=getFFITypeOfId(argsTypes__.GetAt(i).intVal);
        }
        ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, retValType, argsTypes);

        CString absFnName(absFnName_);
        int separatorPos=absFnName.Find("!");
        if(separatorPos==-1)
            return S_FALSE;
        
        CString libName(absFnName.Left(separatorPos));
        CString fnName(absFnName.Mid(separatorPos+1));
        HMODULE hmod=LoadLibrary(libName);
        if(!hmod)
            return S_FALSE;

        FARPROC farproc;        
        farproc=GetProcAddress(hmod, fnName);     
        
        return S_OK;
    }
    /*STDMETHOD(newCIF)()
    {
        // ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_uint, args);
        // must return a CIF object
        // the cif object must have a call method

        // a newStruct would be nice too
        // sockaddr=newStruct([[uint8,"name"], [uint16,"name"])...
        // sockaddr.newStruct(v1,v2,v3);
        // or sockaddr.name1=x;
        // or sockaddr.name2=y;
    }*/
    STDMETHOD(newTCPSocket)(BSTR host, int port, IDispatch **ppSock)
    {
        int iResult;
        addrinfo *result = NULL, *ptr = NULL, hints;
        SOCKET sock = INVALID_SOCKET;

        ZeroMemory( &hints, sizeof(hints) );
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        CString portStr; portStr.Format("%d",port);
        iResult=getaddrinfo(CString(host), portStr, &hints, &result);
        if(iResult!=0)
            goto fail;
        
        sock=socket(result->ai_family, result->ai_socktype, result->ai_protocol);

        iResult=connect( sock, result->ai_addr, (int)result->ai_addrlen);
        if(iResult!=0)
            goto fail;

        freeaddrinfo(result);        

        CComObject<TCPSocketObject> *pTCPSocket=0;
        CComObject<TCPSocketObject>::CreateInstance(&pTCPSocket);
        pTCPSocket->m_sock=sock;

        *ppSock=pTCPSocket;
        (*ppSock)->AddRef();
        return S_OK;
fail:
        *ppSock=0;
        closesocket(sock);
        return S_OK;
    }
};

LRESULT __stdcall ComWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if(msg==WM_USER)
    {
        CComVariant ret;
        CComSafeArray<VARIANT> ary(ULONG(0));
        HRESULT hr=pScriptControl->Run(CComBSTR(L"onOptionsChanged"), ary.GetSafeArrayPtr(), &ret);
        return S_OK;
    } else if(msg==WM_USER+1) {
        CComVariant ret;
        CComSafeArray<VARIANT> ary(ULONG(0));
        ary.Add(CComVariant(wparam));
        ary.Add(CComVariant(lparam));
        HRESULT hr=pScriptControl->Run(CComBSTR(L"onWin32Message"), ary.GetSafeArrayPtr(), &ret);
        return S_OK;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void InitializeScriptControl()
{
    pScriptControl.CreateInstance(__uuidof(ScriptControl));

	pScriptControl->put_AllowUI(VARIANT_TRUE);
    pScriptControl->put_Timeout(-1);
    VARIANT_BOOL noSafeSubset(VARIANT_FALSE);
    pScriptControl->get_UseSafeSubset(&noSafeSubset);
    
    CString language="";
    FILE *f=0;
    if(f=fopen(g_currentDirectory+"\\fscript.js", "rb")) {
        language="JScript";
    } else if(f=fopen(g_currentDirectory+"\\fscript.vbs", "rb")) {
        language="VBScript";
    } else if(f=fopen(g_currentDirectory+"\\fscript.rb", "rb")) {
        language="RubyScript";
    } else if(f=fopen(g_currentDirectory+"\\fscript.py", "rb")) {
        language="Python";
    } else if(f=fopen(g_currentDirectory+"\\fscript.pl", "rb")) {
        language="PerlScript";
    }

    if(!f) {
        MessageBox(0, "Can't open file "+g_currentDirectory+"\\fscript.js.", g_pluginname+" : Error", MB_OK);
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
            MessageBox(0, "Script error in "+g_currentDirectory+"\\fscript.js : "+CString(desc), g_pluginname+" : Error", MB_OK);
            return;
        }
    }

    try {
        CComVariant v;
        pScriptControl->Eval(CComBSTR("displayname"),&v);
        g_pluginname = CString(v);
    } catch(...) {
        MessageBox(0, "displayname variable is missing in "+g_currentDirectory, "FScript error", MB_OK);
    }

    g_comHwnd=CreateWindowEx(0, "Static", "FScript/"+g_pluginname, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    SetWindowLong(g_comHwnd, GWL_WNDPROC, (UINT_PTR)ComWndProc);
}

void ShowOptionDialog() {
    if(g_optionDlgHwnd!=0) {
        SetForegroundWindow(g_optionDlgHwnd);
        return;
    }
    g_optionsDlgDoc.CoCreateInstance(CLSID_DOMDocument);
    VARIANT_BOOL b;
    g_optionsDlgDoc->load(CComVariant(g_currentDirectory+"\\options.xml"), &b);

    CComPtr<IXMLDOMNodeList> pList;
    g_optionsDlgDoc->selectNodes(CComBSTR("options/*"), &pList);
        
    g_optionDlgHwnd=CreateWindowEx(WS_EX_WINDOWEDGE, "#32770", g_pluginname, WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|DS_SETFONT, 0, 0, 0, 0, 0, 0, 0 ,0);
    SetWindowLong(g_optionDlgHwnd, GWL_WNDPROC, (UINT_PTR)FScriptDlgProc);

    HWND hwnd;
    int y=5;
    long len;
    int maxExtent=0;
    pList->get_length(&len);
    for(int i=0;i<len;i++) {
        CComPtr<IXMLDOMNode> pNode;
        pList->get_item(i, &pNode);
        CComPtr<IXMLDOMNamedNodeMap> pAttMap;
        pNode->get_attributes(&pAttMap);
        
        CComPtr<IXMLDOMNode> pLabel;
        pAttMap->getNamedItem(CComBSTR("label"), &pLabel); 
        CComBSTR labelText;
        pLabel->get_text(&labelText);

        CComPtr<IXMLDOMNode> pValue;
        pAttMap->getNamedItem(CComBSTR("value"), &pValue); 
        CComBSTR valueText;
        pValue->get_text(&valueText);

        hwnd=CreateWindowA("Static", CString(labelText), WS_CHILD|WS_VISIBLE, 5, y, 150, 20, g_optionDlgHwnd, (HMENU)0xF000+i, 0, 0);
        SendMessage(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        
        hwnd=CreateWindowA("Edit", CString(valueText), WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL, 160, y, 160, 20, g_optionDlgHwnd, (HMENU)i, 0, 0);
        SendMessage(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        SetWindowLong(hwnd, GWL_WNDPROC, (UINT_PTR)EditProc);
        y+=25;
    }

    hwnd=CreateWindowA("Button", "OK", WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON|WS_TABSTOP, 160, y, 160, 30, g_optionDlgHwnd, (HMENU)0xFFFF, 0, 0);
    SendMessage(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    SetWindowLong(hwnd, GWL_WNDPROC, (UINT_PTR)ButtonProc);

    hwnd=CreateWindowA("Button", "Cancel", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 5, y, 150, 30, g_optionDlgHwnd, (HMENU)0xFFFE, 0, 0);
    SendMessage(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    SetWindowLong(hwnd, GWL_WNDPROC, (UINT_PTR)ButtonProc);
    y+=30;
    
    CPoint pos;
    GetCursorPos(&pos);
    HMONITOR hmon=MonitorFromPoint(pos, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize=sizeof(MONITORINFO);
    GetMonitorInfo(hmon, &mi);        
    
    SendMessage(g_optionDlgHwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    //SelectObject(GetWindowDC(g_optionDlgHwnd), GetStockObject(SYSTEM_FONT));
    SetWindowPos(g_optionDlgHwnd, 0, (mi.rcWork.left+mi.rcWork.right)/2-115, (mi.rcWork.top+mi.rcWork.bottom)/2-(y+30)/2, 330, y+30, SWP_SHOWWINDOW);
}

BOOL MyPlugin_DoInit()
{    
    //OutputDebugString("MyPlugin_DoInit\n");
    FarrAtlModule::m_libid=LIBID_FARRLib;
    HRESULT hr = CoInitialize(NULL);
    GetCurrentDirectory(MAX_PATH, g_currentDirectory.GetBufferSetLength(MAX_PATH));
    g_currentDirectory.ReleaseBuffer();

    GetClassInfo(0, "Edit",   &g_editClass);
    GetClassInfo(0, "Button", &g_btnClass);
    GetClassInfo(0, "#32770", &g_dlgclass);

    InitializeScriptControl();

    CComVariant ret;
    CComSafeArray<VARIANT> ary;
    ary.Add(CComVariant(g_currentDirectory));
    pScriptControl->Run(CComBSTR(L"onInit"), ary.GetSafeArrayPtr(), &ret);

    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    /*if (iResult != 0) {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }*/

    return TRUE;
}
BOOL MyPlugin_DoShutdown()
{
    WSACleanup();
    //OutputDebugString("MyPlugin_DoShutdown\n");
    // success
    pScriptControl->Reset();
    pScriptControl=0;
    if(g_comHwnd)
        DestroyWindow(g_comHwnd);
    if(g_msgHwnd)
        DestroyWindow(g_msgHwnd);
    if(g_optionDlgHwnd)
        DestroyWindow(g_optionDlgHwnd);

    g_pendingTimers.clear();

    CoUninitialize();
    return TRUE;
}
BOOL MyPlugin_GetStrVal(const char* varname,char *destbuf, int maxlen)
{    
    //OutputDebugString("MyPlugin_GetStrVal\n");
    CComVariant ret;
    try
    {
        CComSafeArray<VARIANT> ary;
        ary.Add(CComVariant(CComVariant(varname)));
        pScriptControl->Run(CComBSTR(L"onGetStrValue"), ary.GetSafeArrayPtr(), &ret);
        if(ret.vt!=VT_BSTR) {
            ret.Clear();
            pScriptControl->Eval(CComBSTR(varname),&ret);
        }
        strcpy(destbuf,CString(ret.bstrVal));
        
        return TRUE;
    } catch(...) {
        if(g_currentDirectory!="")
            MessageBox(0, "FARR variable '"+CString(varname)+"' was not returned for "+g_currentDirectory+"\\fscript.js", "FScript error", MB_OK);
        strcpy(destbuf,CString("FScript"));
        return TRUE;
    }
    // not found
    return FALSE;
}
BOOL MyPlugin_SetStrVal(const char* varname, void *val)
{
    //OutputDebugString("MyPlugin_SetStrVal\n");
    // farr host will pass us function pointer we will call
    if (strcmp(varname,DEF_FieldName_NotifySearchCallbackFp)==0)
        callbackfp_notifysearchstatechanged = (Fp_GlobalPluginCallback_NotifySearchStateChanged)val;
    else
    {
        CComVariant ret;
        CComSafeArray<VARIANT> ary;
        ary.Add(CComVariant(CComVariant(varname)));
        ary.Add(CComVariant(CComVariant((char*)val)));
        pScriptControl->Run(CComBSTR(L"onSetStrValue"), ary.GetSafeArrayPtr(), &ret);
        if(ret.vt==VT_BOOL && ret.boolVal == VARIANT_TRUE)
            return TRUE;
    }
    return FALSE;
}
BOOL MyPlugin_SupportCheck(const char* testname, int version)
{
    //OutputDebugString("MyPlugin_SupportCheck\n");
    // ATTN: we support farr interface
    if (strcmp(testname,DEF_FARRAPI_IDENTIFIER)==0)
        return TRUE;

    // otherwise we don't support it
    return FALSE;
}
BOOL MyPlugin_DoAdvConfig()
{
    //OutputDebugString("MyPlugin_DoAdvConfig\n");
    // success
    //ShellExecuteA(NULL, "edit", g_currentDirectory+"\\fscript.js", NULL,NULL, SW_SHOWNORMAL);
    CComVariant ret;
    CComSafeArray<VARIANT> ary; ary.Create();
    HRESULT hr = pScriptControl->Run(CComBSTR(L"onDoAdvConfig"), ary.GetSafeArrayPtr(), &ret);
    if(ret.vt==VT_BOOL && ret.boolVal == VARIANT_TRUE)
        return TRUE;

    ShowOptionDialog();
    return TRUE;
}
BOOL MyPlugin_DoShowReadMe()
{
    //OutputDebugString("MyPlugin_DoShowReadMe\n");
    // by default show the configured readme file
    CComVariant ret;
    CComSafeArray<VARIANT> ary; ary.Create();
    HRESULT hr = pScriptControl->Run(CComBSTR(L"onDoShowReadMe"), ary.GetSafeArrayPtr(), &ret);
    if(ret.vt==VT_BOOL && ret.boolVal == VARIANT_TRUE)
        return TRUE;

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
    //OutputDebugString("MyPlugin_SetState\n");
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
    //OutputDebugString("EFuncName_Ask_WantFeature\n");
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

int GetAPIReleaseNum()
{
    CString releaseNum;
    callbackfp_get_strval(hostptr, "Version.FARR_PLUGINAPI_RELEASENUM", releaseNum.GetBufferSetLength(256), 256); releaseNum.ReleaseBuffer();
    if(releaseNum=="")
        return 0;
    return atoi(releaseNum);
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
// NOTE: if asynchronous searching is to be done, make sure to only set g_current_searchstate=E_SearchState_Stopped when done!
//
PREFUNCDEF BOOL EFuncName_Inform_SearchBegins(const char* searchstring_raw, const char *searchstring_lc_nokeywords, BOOL explicitinvocation)
{
    // don't use this method if release is 3
    if(GetAPIReleaseNum()>=3)
        return FALSE;

    g_results->clear();
    CComVariant ret;
    CComSafeArray<VARIANT> ary;
    ary.Add(CComVariant(CComVariant(g_queryKey)));
    ary.Add(CComVariant(CComVariant(explicitinvocation)));
    ary.Add(CComVariant(CComVariant(searchstring_raw)));
    ary.Add(CComVariant(CComVariant(searchstring_lc_nokeywords)));
    HRESULT hr=pScriptControl->Run(CComBSTR(L"onSearchBegin"), ary.GetSafeArrayPtr(), &ret);

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
// NOTE: if asynchronous searching is to be done, make sure to only set g_current_searchstate=E_SearchState_Stopped when done!
//
PREFUNCDEF BOOL EFuncName_Inform_RegexSearchMatch(const char* searchstring_raw, const char *searchstring_lc_nokeywords, int regexgroups, char** regexcharps)
{
    // don't use this method if release is 3
    if(GetAPIReleaseNum()>=3)
        return FALSE;

    g_results->clear();
    CComVariant ret;
    CComSafeArray<VARIANT> ary;
    ary.Add(CComVariant(CComVariant(g_queryKey)));
    ary.Add(CComVariant(CComVariant(searchstring_raw)));
    ary.Add(CComVariant(CComVariant(searchstring_lc_nokeywords)));
    pScriptControl->Run(CComBSTR(L"onRegexSearchMatch"), ary.GetSafeArrayPtr(), &ret);

    return ret.vt==VT_BOOL && ret.boolVal==VARIANT_TRUE;
}

PREFUNCDEF BOOL EFuncName_Inform_SearchBeginsV2(const char* searchstring_raw, const char *searchstring_lc_nokeywords, BOOL explicitinvocation, const char *modifierstring, int triggermethod)
{
    if(GetAPIReleaseNum()<3)
        return FALSE;

    g_results->clear();
    CComVariant ret;
    CComSafeArray<VARIANT> ary;
    ary.Add(CComVariant(CComVariant(g_queryKey)));
    ary.Add(CComVariant(CComVariant(explicitinvocation)));
    ary.Add(CComVariant(CComVariant(searchstring_raw)));
    ary.Add(CComVariant(CComVariant(searchstring_lc_nokeywords)));
    ary.Add(CComVariant(CComVariant(modifierstring)));
    ary.Add(CComVariant(CComVariant(triggermethod)));
    HRESULT hr=pScriptControl->Run(CComBSTR(L"onSearchBegin"), ary.GetSafeArrayPtr(), &ret);

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
// NOTE: if asynchronous searching is to be done, make sure to only set g_current_searchstate=E_SearchState_Stopped when done!
//
PREFUNCDEF BOOL EFuncName_Inform_RegexSearchMatchV2(const char* searchstring_raw, const char *searchstring_lc_nokeywords, int regexgroups, char** regexcharpsconst, char *modifierstring, int triggermethod)
{
    if(GetAPIReleaseNum()<3)
        return FALSE;

    g_results->clear();
    CComVariant ret;
    CComSafeArray<VARIANT> ary;
    ary.Add(CComVariant(CComVariant(g_queryKey)));
    ary.Add(CComVariant(CComVariant(searchstring_raw)));
    ary.Add(CComVariant(CComVariant(searchstring_lc_nokeywords)));
    ary.Add(CComVariant(CComVariant(modifierstring)));
    ary.Add(CComVariant(CComVariant(triggermethod)));
    pScriptControl->Run(CComBSTR(L"onRegexSearchMatch"), ary.GetSafeArrayPtr(), &ret);

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
    //OutputDebugString("EFuncName_Request_ItemResultByIndex\n");
    strcpy(destbuf_groupname,"");
    *tagvoidpp=NULL;    
    
    if(resultindex>g_results->size())
        return false;

    Result &r((*g_results)[resultindex]);
    strncpy(destbuf_caption, r.title.GetBuffer(), maxlen);
    strncpy(destbuf_path, r.path.GetBuffer(), maxlen);
    strncpy(destbuf_groupname, r.groupname.GetBuffer(), maxlen);

    if(PathFileExists(r.icon))
        strncpy(destbuf_iconfilename, r.icon.GetBuffer(), maxlen);
    else if(PathFileExists(g_currentDirectory+"\\"+r.icon))
        strncpy(destbuf_iconfilename, (g_currentDirectory+"\\"+r.icon).GetBuffer(), maxlen);
    else
        strncpy(destbuf_iconfilename, "", maxlen);
    *tagvoidpp=&r.args;
    *entrytypep=r.entrytype;
    *resultpostprocmodep=r.matching;
    *scorep=r.score;

    //OutputDebugString("\tItem : "+r.title+"\n");

    // ok filled one
    return TRUE;
}


// Host informs us that a search has completed or been interrupted, and we should stop any processing we might be doing assynchronously
//
// NOTE: make sure to set g_current_searchstate=E_SearchState_Stopped;
//
PREFUNCDEF void EFuncName_Inform_SearchEnds()   
{
    //OutputDebugString("EFuncName_Inform_SearchEnds\n");
    g_queryKey++;

    // stop search and set this
    g_current_searchstate=E_SearchState_Stopped;

    // ATTN: test clear results
    g_results->clear();

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
    //OutputDebugString("EFuncName_Ask_IsAnyResultAvailable\n");
    // this will be tracked elsewhere    
    return g_results->size()?E_ResultAvailableState_ItemResuls:E_ResultAvailableState_None;
}

// Host wants to know how many item results are available
PREFUNCDEF int EFuncName_Ask_HowManyItemResultsAreReady()
{
    //OutputDebugString("EFuncName_Ask_HowManyItemResultsAreReady\n");
    // this will be tracked elsewhere
    return g_results->size();
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
    //OutputDebugString("EFuncName_Request_LockResults\n");
    // set lock
    g_current_lockstate=dolock;

    if (dolock==false)
    {
        //for(std::vector<Result>::iterator it=results->begin(); it!=results->end(); it++)
        //    refresults[it->title]=*it;

        if(g_results==&g_tmpResultsA)
            g_results=&g_tmpResultsB;
        else
            g_results=&g_tmpResultsA;
        
        // on unlocking they have retrieved all the results, so CLEAR the results or they will be found again
        g_results->clear();
        // notify host that our state has changed
        ExecuteCallback_SearchStateChanged();        
    }

    // success
    return TRUE;
}
// Host informs us that our window is about to appear
PREFUNCDEF void EFuncName_Inform_WindowIsHiding(HWND hwndp)
{
    //OutputDebugString("EFuncName_Inform_WindowIsHiding\n");
    return;
}

// Host informs us that our window is about to disappear
PREFUNCDEF void EFuncName_Inform_WindowIsUnHiding(HWND hwndp)
{
    //OutputDebugString("EFuncName_Inform_WindowIsUnHiding\n");
    return;
}


// Host wants to know the search state of the plugin
// returns:  E_SearchState_Stopped=0 || E_SearchState_Searching=1
//
// NOTE: this value will be based on a dll global variable
//
PREFUNCDEF E_SearchStateT EFuncName_Ask_SearchState()
{
    //OutputDebugString("EFuncName_Ask_SearchState\n");
    // this will be tracked elsewhere
    return g_current_searchstate;
}

// Host is asking us to return a char* to a LONG result string
//
// NOTE: It is up to us to free this char* when the lock is set to false above OR
//  on the next call, or on search end, but we MUST keep it valid until then.
PREFUNCDEF BOOL EFuncName_Request_TextResultCharp(char **charp)
{
    //OutputDebugString("EFuncName_Request_TextResultCharp\n");
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
PREFUNCDEF BOOL EFuncName_Allow_ProcessTrigger(const char* destbuf_path, const char* destbuf_caption, const char* destbuf_groupname, int pluginid, int thispluginid, int score, E_EntryTypeT entrytype, void* tagvoidp, BOOL *closeafterp)
{
    CComVariant ret;
    CComSafeArray<VARIANT> ary;
    ary.Add(CComVariant(CComVariant(destbuf_path)));
    ary.Add(CComVariant(CComVariant(destbuf_caption)));
    ary.Add(CComVariant(CComVariant(destbuf_groupname)));
    ary.Add(CComVariant(CComVariant(pluginid)));
    ary.Add(CComVariant(CComVariant(thispluginid)));
    ary.Add(CComVariant(CComVariant(score)));
    ary.Add(CComVariant(CComVariant(entrytype)));
    if(tagvoidp && thispluginid==pluginid)
        ary.Add(CComVariant(CComVariant(*(CComVariant*)tagvoidp)));
    else
        ary.Add(CComVariant(CComVariant()));
    pScriptControl->Run(CComBSTR(L"onProcessTrigger"), ary.GetSafeArrayPtr(), &ret);

    static const int TRIGGER_HANDLED=1;
    static const int TRIGGER_CLOSE=2;
    if(ret.vt==VT_I4) {
        *closeafterp=((ret.intVal&TRIGGER_CLOSE)!=0);
        return ((ret.intVal&TRIGGER_HANDLED)!=0);
    }

    return ret.vt==VT_BOOL && ret.boolVal==VARIANT_TRUE;
}

PREFUNCDEF BOOL EFuncName_Allow_ProcessTriggerV2(const char* destbuf_path, const char* destbuf_caption, const char* destbuf_groupname, int pluginid, int thispluginid, int score, E_EntryTypeT entrytype, void* tagvoidp, BOOL *closeafterp, int triggermode)
{
    CComVariant ret;
    CComSafeArray<VARIANT> ary;
    ary.Add(CComVariant(CComVariant(destbuf_path)));
    ary.Add(CComVariant(CComVariant(destbuf_caption)));
    ary.Add(CComVariant(CComVariant(destbuf_groupname)));
    ary.Add(CComVariant(CComVariant(pluginid)));
    ary.Add(CComVariant(CComVariant(thispluginid)));
    ary.Add(CComVariant(CComVariant(score)));
    ary.Add(CComVariant(CComVariant(entrytype)));
    if(tagvoidp && thispluginid==pluginid)
        ary.Add(CComVariant(CComVariant(*(CComVariant*)tagvoidp)));
    else
        ary.Add(CComVariant(CComVariant()));
    ary.Add(CComVariant(CComVariant(triggermode)));
    pScriptControl->Run(CComBSTR(L"onProcessTrigger"), ary.GetSafeArrayPtr(), &ret);

    static const int TRIGGER_HANDLED=1;
    static const int TRIGGER_CLOSE=2;
    if(ret.vt==VT_I4) {
        *closeafterp=((ret.intVal&TRIGGER_CLOSE)!=0);
        return ((ret.intVal&TRIGGER_HANDLED)!=0);
    }

    return ret.vt==VT_BOOL && ret.boolVal==VARIANT_TRUE;
}
//typedef BOOL (*FpFunc_Allow_ProcessTriggerV2)(const char* destbuf_path, const char* destbuf_caption, const char* destbuf_groupname, int pluginid,int thispluginid, int score, E_EntryTypeT entrytype, void* tagvoidp, BOOL *closeafterp, int triggermode);

// Host is asking us if we want to modify the score of an item
// Original score in in *score, we can now modify it if we want
//
// Return TRUE if we modified it, or FALSE if not
//
// NOTE: set score to -9999 to eliminate it from results
//
PREFUNCDEF BOOL EFuncName_Do_AdjustResultScore(const char* itempath, int *scorep)
{
    //OutputDebugString("EFuncName_Do_AdjustResultScore\n");
    // we didnt modify it
    return FALSE;
}

// Helper Functions the plugin calls
void ExecuteCallback_SearchStateChanged()
{
    //CString tmp; tmp.Format("ExecuteCallback_SearchStateChanged.\n\tavailables results : %d\n\tstate : %s\n", g_results->size(), g_current_searchstate==E_SearchState_Stopped?"Stopped":"Searching");
    //OutputDebugString(tmp);
    // tell the host that our state or resultcount has changed
    if (callbackfp_notifysearchstatechanged)
    {
        callbackfp_notifysearchstatechanged(hostptr,g_results->size(),g_current_searchstate);
    }
}

PREFUNCDEF BOOL EFuncName_IdleTime(int elapsedmilliseconds)
{
    CComVariant ret;
    CComSafeArray<VARIANT> ary;
    ary.Add(CComVariant(CComVariant(elapsedmilliseconds)));
    pScriptControl->Run(CComBSTR(L"onIdleTime"), ary.GetSafeArrayPtr(), &ret);

    return ret.vt==VT_BOOL && ret.boolVal==VARIANT_TRUE;
}


PREFUNCDEF BOOL EFuncName_ReceiveKey(int Key, int altpressed, int controlpressed, int shiftpressed)
{
    CComVariant ret;
    CComSafeArray<VARIANT> ary;
    ary.Add(CComVariant(CComVariant(Key)));
    ary.Add(CComVariant(CComVariant(altpressed)));
    ary.Add(CComVariant(CComVariant(controlpressed)));
    ary.Add(CComVariant(CComVariant(shiftpressed)));
    pScriptControl->Run(CComBSTR(L"onReceiveKey"), ary.GetSafeArrayPtr(), &ret);

    return ret.vt==VT_BOOL && ret.boolVal==VARIANT_TRUE;
}
