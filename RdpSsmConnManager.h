#pragma once

#ifndef __AFXWIN_H__
#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h" // Main project identifiers declaration source

class CRdpSsmConnManagerApp : public CWinApp
{
public:
    CRdpSsmConnManagerApp();

public:
    virtual BOOL InitInstance();

    DECLARE_MESSAGE_MAP()
};

extern CRdpSsmConnManagerApp theApp;
