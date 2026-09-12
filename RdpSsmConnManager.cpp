#include "pch.h"
#include "framework.h"
#include "RdpSsmConnManager.h"
#include "RdpSsmConnManagerDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// The singular global application execution instance
CRdpSsmConnManagerApp theApp;

BEGIN_MESSAGE_MAP(CRdpSsmConnManagerApp, CWinApp)
    ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

CRdpSsmConnManagerApp::CRdpSsmConnManagerApp()
{
    // Place single-instance construction parameters here
}

BOOL CRdpSsmConnManagerApp::InitInstance()
{
    // Standard Windows runtime controls registration
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CWinApp::InitInstance();

    // 1. CRITICAL: Initialize core OLE/COM system layers for ActiveX tree/RDP hosting
    if (!AfxOleInit())
    {
        AfxMessageBox(_T("OLE initialization failed! Application cannot start."));
        return FALSE;
    }

    // 2. Enable deep Windows controls layout management routing passes
    AfxEnableControlContainer();

    // Set configuration storage registry profiles (Optional)
    SetRegistryKey(_T("LocalRdpSsmTools"));

    // 3. Construct and launch the interface instance
    CRdpSsmConnManagerDlg dlg;
    m_pMainWnd = &dlg;

    // Opens the dialog window. This call blocks thread processing until the user exits.
    INT_PTR nResponse = dlg.DoModal();

    // Return FALSE so the execution pipeline breaks out cleanly instead of spinning a thread loop
    return FALSE;
}
