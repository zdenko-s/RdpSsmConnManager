#include "pch.h"
#include "framework.h"
#include "RdpSsmConnManager.h"
#include "RdpSsmConnManagerDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CRdpSsmConnManagerDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()
    ON_NOTIFY(TVN_SELCHANGED, 1001, &CRdpSsmConnManagerDlg::OnTvnSelchangedTreeRdg)
    //ON_WM_NCRBUTTONUP()
    ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()


CRdpSsmConnManagerDlg::CRdpSsmConnManagerDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_RDPSSMCONNMANAGER_DIALOG, pParent)
{
    // Initialize our splitter metrics
    m_nTreeWidth = 240;          // Default initial left panel width
    m_bDraggingSplitter = FALSE; // Not dragging initially
    m_nSplitterWidth = 8;        // 8-pixel clickable buffer line gutter
    m_pActiveRdpWnd = nullptr;
}

BOOL CRdpSsmConnManagerDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    if (::AllocConsole())
    {
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
    }

    ModifyStyle(0, WS_CLIPCHILDREN);

    CRect rectClient;
    GetClientRect(&rectClient);
    CRect rectTree(10, 10, m_nTreeWidth, rectClient.Height() - 10);

    m_wndTree.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
        rectTree, this, 1001);

    ParseCommandLineArgs();
    if (!m_strRdgPath.IsEmpty())
    {
        LoadRdgFile(m_strRdgPath);
    }

    return TRUE;
}


void CRdpSsmConnManagerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}


void CRdpSsmConnManagerDlg::ParseCommandLineArgs()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv != nullptr && argc > 1)
    {
        m_strRdgPath = argv[1];
        printf("[INFO] Target Input configuration track localized: %s\n", (const char*)CStringA(m_strRdgPath));
    }
    if (argv) LocalFree(argv);
}


// Helper function to dig out a nested text node value (e.g., properties -> name)
CString GetNestedPropertyValue(IXMLDOMNode* pParentNode, const CString& strTargetTag)
{
    if (!pParentNode) return _T("");

    CComPtr<IXMLDOMNodeList> spChildren;
    if (SUCCEEDED(pParentNode->get_childNodes(&spChildren)) && spChildren)
    {
        long nLength = 0;
        spChildren->get_length(&nLength);
        for (long i = 0; i < nLength; ++i)
        {
            CComPtr<IXMLDOMNode> spChild;
            spChildren->get_item(i, &spChild);
            if (!spChild) continue;

            CComBSTR bstrName;
            spChild->get_nodeName(&bstrName);
            if (bstrName == (LPCOLESTR)strTargetTag)
            {
                CComBSTR bstrText;
                spChild->get_text(&bstrText);
                return CString(bstrText);
            }
        }
    }
    return _T("");
}

// Scans an object node for a nested `<properties>` block to find its descriptive label
CString ExtractNameFromProperties(IXMLDOMNode* pObjectNode, const CString& strPreferredTag)
{
    if (!pObjectNode) return _T("");

    CComPtr<IXMLDOMNodeList> spChildren;
    if (SUCCEEDED(pObjectNode->get_childNodes(&spChildren)) && spChildren)
    {
        long nLength = 0;
        spChildren->get_length(&nLength);
        for (long i = 0; i < nLength; ++i)
        {
            CComPtr<IXMLDOMNode> spChild;
            spChildren->get_item(i, &spChild);
            if (!spChild) continue;

            CComBSTR bstrName;
            spChild->get_nodeName(&bstrName);
            if (bstrName == L"properties")
            {
                // Try preferred tag first (e.g., displayName), fall back to standard 'name'
                CString strVal = GetNestedPropertyValue(spChild, strPreferredTag);
                if (strVal.IsEmpty())
                {
                    strVal = GetNestedPropertyValue(spChild, _T("name"));
                }
                return strVal;
            }
        }
    }
    return _T("");
}

// Helper to scrape a port number off the tail of a display text line
int ExtractPortFromString(const CString& strText)
{
    int nFindPos = strText.Find(_T("Port: "));
    if (nFindPos != -1)
    {
        CString strPort = strText.Mid(nFindPos + 6);
        int nEndPos = strPort.Find(_T(")"));
        if (nEndPos != -1) strPort = strPort.Left(nEndPos);
        return _wtoi(strPort);
    }

    int nLen = strText.GetLength();
    int i = nLen - 1;
    while (i >= 0 && _istdigit(strText[i])) { i--; }
    if (i < nLen - 1)
    {
        return _wtoi(strText.Mid(i + 1));
    }
    return 3389;
}

void ProcessGroupNode(IXMLDOMNode* pGroupNode, CTreeCtrl& tree, HTREEITEM hParentItem)
{
    if (!pGroupNode) return;

    CComBSTR bstrNodeName;
    pGroupNode->get_nodeName(&bstrNodeName);
    if (bstrNodeName == L"#text" || bstrNodeName == L"#comment") return;

    // 1. Read the group's name from its nested <properties><name> tags
    CString strGroupName = ExtractNameFromProperties(pGroupNode, _T("name"));
    if (strGroupName.IsEmpty()) strGroupName = _T("Unnamed Group");

    // 2. Add group element entry to the tree layout
    HTREEITEM hCurrentGroupItem = tree.InsertItem(strGroupName, hParentItem);

    // 3. Process sub-elements nested within this group
    CComPtr<IXMLDOMNodeList> spChildren;
    if (SUCCEEDED(pGroupNode->get_childNodes(&spChildren)) && spChildren)
    {
        long nLength = 0;
        spChildren->get_length(&nLength);
        for (long i = 0; i < nLength; ++i)
        {
            CComPtr<IXMLDOMNode> spChild;
            spChildren->get_item(i, &spChild);
            if (!spChild) continue;

            CComBSTR bstrChildName;
            spChild->get_nodeName(&bstrChildName);

            if (bstrChildName == L"group")
            {
                // Recursively follow sub-groups
                ProcessGroupNode(spChild, tree, hCurrentGroupItem);
            }
            else if (bstrChildName == L"server")
            {
                // Read server identity out of <properties><displayName> tags
                CString strServerText = ExtractNameFromProperties(spChild, _T("displayName"));
                if (strServerText.IsEmpty()) strServerText = _T("Unknown Server");

                HTREEITEM hServerItem = tree.InsertItem(strServerText, hCurrentGroupItem);

                // Parse port out of the display name and cache it inside the tree item
                int nExtractedPort = ExtractPortFromString(strServerText);
                tree.SetItemData(hServerItem, (DWORD_PTR)nExtractedPort);
            }
        }
    }
}

void CRdpSsmConnManagerDlg::LoadRdgFile(const CString& strPath)
{
    printf("[PARSER] Initializing MSXML DOM Document Loader for RDCMan schema...\n");
    m_wndTree.DeleteAllItems();

    CComPtr<IXMLDOMDocument> spDoc;

    // CHANGE: Swap CLSID_DOMDocument60 out for __uuidof(DOMDocument60)
    HRESULT hr = spDoc.CoCreateInstance(__uuidof(DOMDocument60));

    if (FAILED(hr) || !spDoc)
    {
        printf("[ERROR] Failed to allocate native MSXML6.0 engine framework instance. HRESULT: 0x%08X\n", hr);
        return;
    }

    spDoc->put_async(VARIANT_FALSE);
    spDoc->put_validateOnParse(VARIANT_FALSE);
    spDoc->put_resolveExternals(VARIANT_FALSE);

    CComBSTR bstrFilePath(strPath);
    CComVariant varPath(bstrFilePath);

    VARIANT_BOOL bSuccess = VARIANT_FALSE;

    hr = spDoc->load(varPath, &bSuccess);
    if (FAILED(hr) || bSuccess == VARIANT_FALSE)
    {
        printf("[ERROR] Could not read file path profile stream. Verify file exists.\n");
        m_wndTree.InsertItem(_T("Error loading .rdg profile file"));
        return;
    }

    CComPtr<IXMLDOMElement> spRoot;
    spDoc->get_documentElement(&spRoot);
    if (!spRoot) return;

    printf("[SUCCESS] File loaded into DOM. Parsing structural nodes...\n");

    // Drill past the topmost root <file> node container to process groups
    CComPtr<IXMLDOMNodeList> spFileElements;
    if (SUCCEEDED(spRoot->get_childNodes(&spFileElements)) && spFileElements)
    {
        long nLength = 0;
        spFileElements->get_length(&nLength);
        for (long i = 0; i < nLength; ++i)
        {
            CComPtr<IXMLDOMNode> spNode;
            spFileElements->get_item(i, &spNode);
            if (!spNode) continue;

            CComBSTR bstrName;
            spNode->get_nodeName(&bstrName);

            // Look for the main file envelope container node
            if (bstrName == L"file")
            {
                CComPtr<IXMLDOMNodeList> spFileChildren;
                if (SUCCEEDED(spNode->get_childNodes(&spFileChildren)) && spFileChildren)
                {
                    long nFileChildLen = 0;
                    spFileChildren->get_length(&nFileChildLen);
                    for (long j = 0; j < nFileChildLen; ++j)
                    {
                        CComPtr<IXMLDOMNode> spFileChild;
                        spFileChildren->get_item(j, &spFileChild);

                        CComBSTR bstrChildType;
                        if (spFileChild && SUCCEEDED(spFileChild->get_nodeName(&bstrChildType)) && bstrChildType == L"group")
                        {
                            // Map the high-level group elements (Regions) directly to our tree UI root
                            ProcessGroupNode(spFileChild, m_wndTree, TVI_ROOT);
                        }
                    }
                }
            }
        }
    }
    printf("[PARSER SUCCESS] Hierarchical layout populated onto Tree Component framework.\n");

    // --- EXPAND FIRST LEVEL ONLY ---
    printf("[UI] Expanding topmost root nodes (AWS Regions)...\n");

    // Get the first item at the very top of the tree
    HTREEITEM hRootItem = m_wndTree.GetRootItem();

    while (hRootItem != nullptr)
    {
        // Expand the top-level region item (this does NOT recursively expand children)
        m_wndTree.Expand(hRootItem, TVE_EXPAND);

        // Move to the next topmost sibling node (next region)
        hRootItem = m_wndTree.GetNextSiblingItem(hRootItem);
    }

    printf("[UI SUCCESS] Top level layout initialized cleanly.\n");
}

CWnd* CRdpSsmConnManagerDlg::InitializeRdpControl(HWND hwndParent, const CRect& rect, int port)
{
    CWnd* pWnd = CWnd::FromHandle(hwndParent);
    if (!pWnd) return nullptr;

    LPUNKNOWN pUnk = pWnd->GetControlUnknown();
    if (pUnk == nullptr) return nullptr;

    CComDispatchDriver rdpDisp(pUnk);
    CComVariant varServer(L"127.0.0.1");
    rdpDisp.PutPropertyByName(L"Server", &varServer);

    // --- FIX: Eliminate hardcoded 1280x800 values. ---
    // Natively query the target container's current real-time dimension properties.
    // Whether minimized, regular, or maximized, it boots at an exact 1:1 pixel match!
    CComVariant varWidth(rect.Width());
    CComVariant varHeight(rect.Height());
    rdpDisp.PutPropertyByName(L"DesktopWidth", &varWidth);
    rdpDisp.PutPropertyByName(L"DesktopHeight", &varHeight);

    printf("[PARSER] Requesting initialization canvas resolution: %dx%d\n", rect.Width(), rect.Height());

    CComVariant varAdvanced;
    HRESULT hr = rdpDisp.GetPropertyByName(L"AdvancedSettings9", &varAdvanced);
    if (FAILED(hr) || varAdvanced.vt != VT_DISPATCH || varAdvanced.pdispVal == nullptr)
    {
        rdpDisp.GetPropertyByName(L"AdvancedSettings", &varAdvanced);
    }

    if (varAdvanced.vt == VT_DISPATCH && varAdvanced.pdispVal != nullptr)
    {
        CComDispatchDriver advDisp(varAdvanced.pdispVal);
        CComVariant varPort(port);
        CComVariant varSmartSize(VARIANT_TRUE);
        CComVariant varAuthLevel(0);
        CComVariant varCredSSP(VARIANT_TRUE);

        advDisp.PutPropertyByName(L"RDPPort", &varPort);
        advDisp.PutPropertyByName(L"SmartSizing", &varSmartSize);
        advDisp.PutPropertyByName(L"AuthenticationLevel", &varAuthLevel);
        advDisp.PutPropertyByName(L"EnableCredSspSupport", &varCredSSP);
    }

    printf("[EXECUTION] Handshaking background session on Port Route: %d...\n", port);
    rdpDisp.Invoke0(L"Connect");

    return pWnd;
}


void CRdpSsmConnManagerDlg::OnTvnSelchangedTreeRdg(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
    HTREEITEM hSelected = pNMTreeView->itemNew.hItem;

    if (hSelected != nullptr)
    {
        DWORD_PTR dwData = m_wndTree.GetItemData(hSelected);
        int nSelectedPort = (int)dwData;

        if (nSelectedPort > 0)
        {
            printf("[SELECTION] Shifting viewport matrix for Target Port: %d\n", nSelectedPort);

            CRect rectClient;
            GetClientRect(&rectClient);
            // This is our visible RDP canvas zone on the right side of the split screen
            CRect rectVisibleZone(260, 10, rectClient.Width() - 10, rectClient.Height() - 10);

            // 1. SLIDE OFF-SCREEN: Instead of hiding, slide the currently active window completely out of bounds
            if (m_pActiveRdpWnd != nullptr)
            {
                // Moving it to -20000 keeps it fully alive and painting in memory, but hidden to the user
                m_pActiveRdpWnd->MoveWindow(-20000, 10, rectVisibleZone.Width(), rectVisibleZone.Height());
                m_pActiveRdpWnd = nullptr;
            }

            CWnd* pTargetRdpWnd = nullptr;

            // 2. SESSION LOOK-UP
            if (m_mapSessions.Lookup(hSelected, pTargetRdpWnd) && pTargetRdpWnd != nullptr)
            {
                printf("[SESSION INFRA] Found active background session canvas. Re-centering viewport instantly...\n");
                m_pActiveRdpWnd = pTargetRdpWnd;
            }
            else
            {
                printf("[SESSION INFRA] Spawning fresh isolated session instance container...\n");

                CWnd* pNewWnd = new CWnd();
                // FIX: Instantiate the control visible natively right from the start
                BOOL bCreated = pNewWnd->CreateControl(L"MsTscAx.MsTscAx", nullptr,
                    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                    rectVisibleZone, this, 2000 + nSelectedPort);

                if (bCreated)
                {
                    InitializeRdpControl(pNewWnd->GetSafeHwnd(), rectVisibleZone, nSelectedPort);
                    m_mapSessions.SetAt(hSelected, pNewWnd);
                    m_pActiveRdpWnd = pNewWnd;
                }
                else
                {
                    printf("[ERROR] Allocation failure creating multi-session interface container.\n");
                    delete pNewWnd;
                }
            }

            // 3. SLIDE ON-SCREEN: Snap the selected session window directly back into the visible canvas zone
            if (m_pActiveRdpWnd != nullptr)
            {
                m_pActiveRdpWnd->MoveWindow(rectVisibleZone.left, rectVisibleZone.top,
                    rectVisibleZone.Width(), rectVisibleZone.Height());
                m_pActiveRdpWnd->Invalidate();
                m_pActiveRdpWnd->UpdateWindow();
            }
        }
    }

    *pResult = 0;
}


void CRdpSsmConnManagerDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    RearrangeControls(cx, cy);
}

void CRdpSsmConnManagerDlg::RearrangeControls(int cx, int cy)
{
    // Ensure window structures are fully online before attempting adjustments
    if (!m_wndTree.GetSafeHwnd()) return;

    // Rescale left tree panel
    m_wndTree.MoveWindow(10, 10, m_nTreeWidth, cy - 20);

    // Calculate remaining right-side canvas coordinate anchor points
    int nRdpLeft = 10 + m_nTreeWidth + m_nSplitterWidth;
    int nRdpWidth = cx - nRdpLeft - 10;
    int nRdpHeight = cy - 20;

    if (nRdpWidth <= 0 || nRdpHeight <= 0) return;

    // Loop and apply the updated dimensions to all running session containers
    if (m_mapSessions.GetCount() > 0)
    {
        POSITION pos = m_mapSessions.GetStartPosition();
        while (pos != nullptr)
        {
            HTREEITEM hKey;
            CWnd* pWnd = nullptr;
            m_mapSessions.GetNextAssoc(pos, hKey, pWnd);

            if (pWnd && pWnd->GetSafeHwnd())
            {
                if (pWnd == m_pActiveRdpWnd)
                {
                    // Stretch the active window to perfectly fit the resized container
                    pWnd->MoveWindow(nRdpLeft, 10, nRdpWidth, nRdpHeight);
                }
                else
                {
                    // Preserves background off-screen alignment at -32000 
                    // and updates its width/height so it's ready when switched back
                    pWnd->MoveWindow(-32000, -32000, nRdpWidth, nRdpHeight);
                }
            }
        }
    }
}


void CRdpSsmConnManagerDlg::OnPaint()
{
    CPaintDC dc(this);
    CDialogEx::OnPaint();
}

void CRdpSsmConnManagerDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    // Check if the user clicked inside the vertical gutter zone right next to the tree control
    int nSplitterLeft = 10 + m_nTreeWidth;
    int nSplitterRight = nSplitterLeft + m_nSplitterWidth;

    if (point.x >= nSplitterLeft && point.x <= nSplitterRight)
    {
        m_bDraggingSplitter = TRUE;
        SetCapture(); // Lock mouse inputs directly to this window structure while dragging
    }

    CDialogEx::OnLButtonDown(nFlags, point);
}

void CRdpSsmConnManagerDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDraggingSplitter)
    {
        m_bDraggingSplitter = FALSE;
        ReleaseCapture(); // Give mouse tracking back to normal operations
    }

    CDialogEx::OnLButtonUp(nFlags, point);
}

void CRdpSsmConnManagerDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bDraggingSplitter)
    {
        CRect rectClient;
        GetClientRect(&rectClient);

        // Keep resizing boundaries within practical human dimensions
        if (point.x > 100 && point.x < (rectClient.Width() - 200))
        {
            m_nTreeWidth = point.x - 10; // Calculate new tree dimension offset

            // Re-render and stretch the layout boxes immediately
            RearrangeControls(rectClient.Width(), rectClient.Height());
        }
    }

    CDialogEx::OnMouseMove(nFlags, point);
}

BOOL CRdpSsmConnManagerDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    CPoint point;
    ::GetCursorPos(&point);
    ScreenToClient(&point);

    int nSplitterLeft = 10 + m_nTreeWidth;
    int nSplitterRight = nSplitterLeft + m_nSplitterWidth;

    // Change system arrow cursor into a East/West resize pointer when hovering over the splitter line
    if ((point.x >= nSplitterLeft && point.x <= nSplitterRight) || m_bDraggingSplitter)
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_SIZEWE));
        return TRUE; // Intercept event routing
    }

    return CDialogEx::OnSetCursor(pWnd, nHitTest, message);
}

/*
void CRdpSsmConnManagerDlg::OnNcRButtonUp(UINT nHitTest, CPoint point) {
    if (nHitTest == HTCAPTION) {
        if (m_mapSessions.IsEmpty()) return;

        CMenu popMenu;
        popMenu.CreatePopupMenu();

        HTREEITEM hKeyItem = NULL;  // Matching your map's KEY type
        CWnd* pValueWnd = nullptr;  // Matching your map's VALUE type
        POSITION pos = m_mapSessions.GetStartPosition();

        UINT menuID = IDM_SWITCH_SESSIONS_START;

        while (pos != NULL && menuID <= IDM_SWITCH_SESSIONS_END) {
            m_mapSessions.GetNextAssoc(pos, hKeyItem, pValueWnd);

            // Fetch the human-readable server name string directly from the tree node
            CString sServerName = m_wndTree.GetItemText(hKeyItem);

            UINT flags = MF_STRING;
            if (pValueWnd == m_pActiveRdpWnd) {
                flags |= MF_CHECKED;
            }

            popMenu.AppendMenu(flags, menuID, sServerName);
            menuID++;
        }

        popMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        return;
    }

    CDialogEx::OnNcRButtonUp(nHitTest, point);
}
*/

BOOL CRdpSsmConnManagerDlg::OnCommand(WPARAM wParam, LPARAM lParam) {
    UINT nID = LOWORD(wParam);

    if (nID >= IDM_SWITCH_SESSIONS_START && nID <= IDM_SWITCH_SESSIONS_END) {
        UINT targetIndex = nID - IDM_SWITCH_SESSIONS_START;

        if (!m_mapSessions.IsEmpty()) {
            HTREEITEM hKeyItem = NULL;
            CWnd* pValueWnd = nullptr;
            POSITION pos = m_mapSessions.GetStartPosition();

            UINT currentIndex = 0;
            bool bFound = false;

            while (pos != NULL) {
                m_mapSessions.GetNextAssoc(pos, hKeyItem, pValueWnd);

                if (currentIndex == targetIndex) {
                    bFound = true;
                    break;
                }
                currentIndex++;
            }

            if (bFound && pValueWnd != nullptr && pValueWnd != m_pActiveRdpWnd) {
                CRect rectRdpPane;

                if (m_pActiveRdpWnd != nullptr) {
                    // 1. Get the precise right-side screen coordinates from your current active window
                    m_pActiveRdpWnd->GetWindowRect(&rectRdpPane);
                    ScreenToClient(&rectRdpPane);

                    // 2. Teleport the current active window far away to -32000 so it doesn't disconnect
                    m_pActiveRdpWnd->SetWindowPos(NULL, -32000, -32000, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
                }
                else {
                    // Fallback for the first session setup:
                    // Take up the entire right side of the dialog, starting exactly where the tree ends
                    GetClientRect(&rectRdpPane);
                    CRect rectTree;
                    m_wndTree.GetWindowRect(&rectTree);
                    ScreenToClient(&rectTree);

                    // The left side of the RDP pane is the right side of the tree + a small margin
                    rectRdpPane.left = rectTree.right + 5;
                }

                // 3. Teleport your target background window back to the active display panel coordinates
                pValueWnd->SetWindowPos(NULL, rectRdpPane.left, rectRdpPane.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

                // 4. Update your active variable pointer tracker
                m_pActiveRdpWnd = pValueWnd;

                // 5. Update the left side selection bar
                if (hKeyItem != NULL) {
                    m_wndTree.SelectItem(hKeyItem);
                    m_wndTree.EnsureVisible(hKeyItem);
                }
            }
        }
        return TRUE;
    }

    return CDialogEx::OnCommand(wParam, lParam);
}

void CRdpSsmConnManagerDlg::OnContextMenu(CWnd* pWnd, CPoint point)
{
    // 1. Determine exactly what region of the dialog window was clicked
    // Convert screen cursor points back to window tracking points
    CPoint clientPt = point;
    ScreenToClient(&clientPt);

    // Check if the click coordinates hit the native Window Title Bar / Caption area
    auto nHitTest = SendMessage(WM_NCHITTEST, 0, MAKELPARAM(point.x, point.y));

    if (nHitTest == HTCAPTION)
    {
        // If no background tunnels are running, do nothing (or fallback to standard menu)
        if (m_mapSessions.IsEmpty()) return;

        CMenu popMenu;
        popMenu.CreatePopupMenu(); //

        HTREEITEM hKeyItem = NULL;
        CWnd* pValueWnd = nullptr;
        POSITION pos = m_mapSessions.GetStartPosition();

        UINT menuID = IDM_SWITCH_SESSIONS_START;

        // Populate your open background tunnels inside your CMap
        while (pos != NULL && menuID <= IDM_SWITCH_SESSIONS_END)
        {
            m_mapSessions.GetNextAssoc(pos, hKeyItem, pValueWnd);

            // Fetch the server name dynamically from your tree using the key handle
            CString sServerName = m_wndTree.GetItemText(hKeyItem);

            UINT flags = MF_STRING;
            // Place a checkmark next to the active view pane
            if (pValueWnd == m_pActiveRdpWnd)
            {
                flags |= MF_CHECKED;
            }

            popMenu.AppendMenu(flags, menuID, sServerName); //
            menuID++;
        }

        // 2. Display your custom view switcher directly under the user's cursor
        popMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this); //

        return; // <-- CRITICAL: Stops message routing so the default System Menu is BLOCKED
    }

    // Pass right-clicks on other sections (like inside the tree) to the baseline default handler
    CDialogEx::OnContextMenu(pWnd, point);
}
