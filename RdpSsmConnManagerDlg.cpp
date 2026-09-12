#include "pch.h"
#include "framework.h"
#include "RdpSsmConnManager.h"
#include "RdpSsmConnManagerDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <initguid.h>

// Native GUID for IMsRdpClientNonScriptable5
DEFINE_GUID(IID_IMsRdpClientNonScriptable5, 0x4f42c070, 0x50d5, 0x4700, 0x99, 0x93, 0x27, 0x0b, 0x20, 0x14, 0x1f, 0x2a);

// Define the interface struct manually if your Windows SDK header lacks it
MIDL_INTERFACE("4f42c070-50d5-4700-9993-270b20141f2a")
IMsRdpClientNonScriptable5 : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetPropertyByName(BSTR, VARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE PutPropertyByName(BSTR, VARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE NotifySizeChange(void) = 0; // Legacy
    virtual HRESULT STDMETHODCALLTYPE UpdateSessionDisplaySettings(void) = 0; // The Holy Grail
};



#define IDC_RDP_CTRL_START   20000
#define IDC_RDP_CTRL_END     20050 // Supports up to 50 concurrent sessions
#define WM_POST_INITIALIZE_RDP    (WM_USER + 102)

BEGIN_MESSAGE_MAP(CRdpSsmConnManagerDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()
    ON_NOTIFY(TVN_SELCHANGED, 1001, &CRdpSsmConnManagerDlg::OnTvnSelchangedTreeRdg)
    ON_WM_CONTEXTMENU()
    ON_WM_TIMER()
    ON_WM_SETCURSOR()
    ON_WM_MOUSEMOVE()
    ON_MESSAGE(WM_POST_INITIALIZE_RDP, &CRdpSsmConnManagerDlg::OnPostInitializeRdp)
    ON_NOTIFY(NM_CLICK, IDC_TREE_RDG, &CRdpSsmConnManagerDlg::OnNMClickTreeRdg)
    ON_NOTIFY(NM_DBLCLK, IDC_TREE_RDG, &CRdpSsmConnManagerDlg::OnNMDblclkTreeRdg)
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()
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
        rectTree, this, IDC_TREE_RDG);

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

    // Enforce your mandatory localhost address routing for AWS SSM tunnels
    CComVariant varServer(L"127.0.0.1");
    rdpDisp.PutPropertyByName(L"Server", &varServer);

    // Fetch the correct advanced settings block
    CComVariant varAdvanced;
    if (SUCCEEDED(rdpDisp.GetPropertyByName(L"AdvancedSettings7", &varAdvanced)) && varAdvanced.vt == VT_DISPATCH)
    {
        CComDispatchDriver advDisp(varAdvanced.pdispVal);
        CComVariant varPort(port);
        CComVariant varSmartSize(VARIANT_TRUE); // <-- Forces wall-to-wall canvas mapping natively
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

            // 1. PARK PREVIOUS SESSION: Teleport old active windows out of view
            if (m_pActiveRdpWnd != nullptr)
            {
                m_pActiveRdpWnd->SetWindowPos(NULL, -32000, -32000, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
                m_pActiveRdpWnd = nullptr;
            }

            CWnd* pTargetRdpWnd = nullptr;

            // 2. LOGICAL BRANCH A: The session is already open and running in the background!
            if (m_mapSessions.Lookup(hSelected, pTargetRdpWnd) && pTargetRdpWnd != nullptr)
            {
                printf("[SESSION INFRA] Found active background session canvas. Re-centering viewport instantly...\n");
                m_bTreeVisible = false;
                m_pActiveRdpWnd = pTargetRdpWnd;
                RearrangeControls(rectClient.Width(), rectClient.Height());
            }
            // 3. LOGICAL BRANCH B: This is a brand new session activation request!
            else
            {
                printf("[SESSION INFRA] Spawning fresh isolated session instance container...\n");

                // --- STEP A: COLLAPSE TREE PANEL BEFORE CREATION ---
                // This forces the tree off-screen instantly, freeing up 100% application width
                m_bTreeVisible = false;

                // Temporarily bypass the connection safety flag so RearrangeControls can scale the layout
                RearrangeControls(rectClient.Width(), rectClient.Height());

                // --- STEP B: COMPUTE TRUE MAXIMUM WALL-TO-WALL CANVAS BOUNDS ---
                int nRdpLeft = 5; // Left edge margin since tree is now collapsed
                int nRdpWidth = rectClient.Width() - nRdpLeft - 10;
                int nRdpHeight = rectClient.Height() - 20;

                CRect rectFullWallToWall(nRdpLeft, 10, nRdpLeft + nRdpWidth, 10 + nRdpHeight);
                printf("[LAUNCHER] Creating control container directly wall-to-wall: Width=%d, Height=%d\n",
                    rectFullWallToWall.Width(), rectFullWallToWall.Height());

                CWnd* pNewWnd = new CWnd();

                // Create the control container shell natively at the full, maximum width bounds
                BOOL bCreated = pNewWnd->CreateControl(L"MsTscAx.MsTscAx", nullptr,
                    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                    rectFullWallToWall, this, 2000 + nSelectedPort);

                if (bCreated)
                {
                    printf("[LAUNCHER] CreateControl completed successfully at full canvas size.\n");

                    m_mapSessions.SetAt(hSelected, pNewWnd);
                    m_pActiveRdpWnd = pNewWnd;

                    // Cache details for our delayed asynchronous worker tick
                    m_hPendingSelectedNode = hSelected;
                    m_nPendingSelectedPort = nSelectedPort;
                    m_rectPendingZone = rectFullWallToWall;

                    // Start the 10ms single-shot async delay timer to safely clear out of the tree event thread
                    this->SetTimer(IDC_LAUNCH_DELAY_TIMER, 10, nullptr);
                    printf("[LAUNCHER] Delayed kickstarter timer queued successfully.\n");
                }
                else
                {
                    printf("[LAUNCHER] ERROR: CreateControl failed to instantiate ActiveX control!\n");
                    delete pNewWnd;
                }
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

    int nTreeX = 0;

    // --- THE OVERLAY FIX ---
    // The RDP window ALWAYS starts at the absolute left edge of the canvas,
    // regardless of whether the tree is open or closed!
    int nRdpLeft = 5;

    if (!m_bTreeVisible)
    {
        // Slide the tree completely out of view to negative coordinates
        nTreeX = -m_nTreeWidth - 10;
    }
    else
    {
        // Tree slides directly on top of the RDP control at X = 0
        nTreeX = 0;
    }

    // Calculate maximum available canvas area width (Always full size!)
    int nRdpWidth = cx - nRdpLeft - 10;
    int nRdpHeight = cy - 20;

    if (nRdpWidth <= 0 || nRdpHeight <= 0) return;

    // 1. Move the RDP session container windows first
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
                    // Scale the active window wall-to-wall
                    pWnd->MoveWindow(nRdpLeft, 10, nRdpWidth, nRdpHeight);
                }
                else
                {
                    // Park background sessions safely out of view
                    pWnd->MoveWindow(-32000, -32000, nRdpWidth, nRdpHeight);
                }
            }
        }
    }

    // 2. CRITICAL STEP: Move the tree panel LAST and force it to the top of the Z-order
    // This ensures it floats directly on top of the RDP control instead of hiding behind it
    m_wndTree.MoveWindow(nTreeX, 10, m_nTreeWidth, cy - 20);
    m_wndTree.SetWindowPos(&CWnd::wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}


void CRdpSsmConnManagerDlg::OnPaint()
{
    CPaintDC dc(this);
    CDialogEx::OnPaint();
}

void CRdpSsmConnManagerDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    // If tree is open and the click hits within 5 pixels of the right border
    if (m_bTreeVisible && (point.x >= m_nTreeWidth - 5 && point.x <= m_nTreeWidth + 5))
    {
        m_bIsDraggingEdge = true;
        SetCapture(); // Lock mouse inputs strictly to this dialog container frame
        return;
    }

    CDialogEx::OnLButtonDown(nFlags, point);
}

void CRdpSsmConnManagerDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bIsDraggingEdge)
    {
        printf("[MOUSE] Drag complete. Releasing window input locks.\n");
        m_bIsDraggingEdge = false;
        ReleaseCapture(); // Give mouse tracking controls back cleanly to the operating system loop
        return;
    }

    CDialogEx::OnLButtonUp(nFlags, point);
}
void CRdpSsmConnManagerDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bIsDraggingEdge)
    {
        // Enforce safe structural layout resizing barriers
        if (point.x < m_nMinTreeWidth) point.x = m_nMinTreeWidth;
        if (point.x > m_nMaxTreeWidth) point.x = m_nMaxTreeWidth;

        // Update the width variable dynamically in memory
        m_nTreeWidth = point.x;

        // Recalculate layout borders instantly on the fly
        CRect rectClient;
        GetClientRect(&rectClient);
        RearrangeControls(rectClient.Width(), rectClient.Height());
        return;
    }

    CDialogEx::OnMouseMove(nFlags, point);
}

BOOL CRdpSsmConnManagerDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    CPoint ptCursor;
    GetCursorPos(&ptCursor);
    ScreenToClient(&ptCursor);

    CRect rectClient;
    GetClientRect(&rectClient);

    // 1. RESIZE ZONE CHECK: If tree is visible, check if mouse is on its right edge (+/- 5 pixels)
    if (m_bTreeVisible && (ptCursor.x >= m_nTreeWidth - 5 && ptCursor.x <= m_nTreeWidth + 5))
    {
        // Change the cursor to the classic horizontal resize arrow
        ::SetCursor(::LoadCursor(NULL, IDC_SIZEWE));
        return TRUE; // Intercept message routing to stop default cursor resets
    }

    // 2. DRAG SAFETY LOCK: If the user is actively dragging, lock the tree open and touch nothing
    if (m_bIsDraggingEdge)
    {
        ::SetCursor(::LoadCursor(NULL, IDC_SIZEWE));
        return TRUE;
    }

    // 3. AUTO-HIDE SLIDE TOGGLES: (Your existing edge-hover mechanics)
    // Bumps extreme left edge -> Slide open
    if (!m_bTreeVisible && ptCursor.x <= 5 && ptCursor.y >= 0 && ptCursor.y <= rectClient.Height())
    {
        m_bTreeVisible = true;
        RearrangeControls(rectClient.Width(), rectClient.Height());
    }
    // Moves out of the tree panel zone -> Collapse shut
    else if (m_bTreeVisible && ptCursor.x > (m_nTreeWidth + 20))
    {
        m_bTreeVisible = false;
        RearrangeControls(rectClient.Width(), rectClient.Height());
    }

    return CDialogEx::OnSetCursor(pWnd, nHitTest, message);
}

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
    auto nHitTest = SendMessage(WM_NCHITTEST, 0, MAKELPARAM(point.x, point.y));

    if (nHitTest == HTCAPTION)
    {
        printf("[RDP-MENU] Title bar right-clicked. Map has %d items.\n", (int)m_mapSessions.GetCount());

        if (m_mapSessions.IsEmpty()) return;

        CMenu popMenu;
        popMenu.CreatePopupMenu();

        HTREEITEM hKeyItem = NULL;
        CWnd* pValueWnd = nullptr;
        POSITION pos = m_mapSessions.GetStartPosition();

        UINT menuID = IDM_SWITCH_SESSIONS_START;

        while (pos != NULL && menuID <= IDM_SWITCH_SESSIONS_END)
        {
            m_mapSessions.GetNextAssoc(pos, hKeyItem, pValueWnd);
            CString sServerName = m_wndTree.GetItemText(hKeyItem);

            printf("[RDP-MENU] Injecting item into switcher list menu: '%s' (ID: %u)\n", (LPCSTR)CT2A(sServerName), menuID);

            UINT flags = MF_STRING;
            if (pValueWnd == m_pActiveRdpWnd)
            {
                flags |= MF_CHECKED;
            }

            popMenu.AppendMenu(flags, menuID, sServerName);
            menuID++;
        }

        popMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        return;
    }

    CDialogEx::OnContextMenu(pWnd, point);
}

void CRdpSsmConnManagerDlg::OnRdpDisconnected(UINT nID, long discReason)
{
    if (m_mapSessions.IsEmpty())
        return;

    HTREEITEM hTargetKeyItem = NULL;
    CWnd* pTargetValueWnd = nullptr;
    bool bFound = false;

    // 1. Scan your CMap to identify which tree node owns this specific control ID
    POSITION pos = m_mapSessions.GetStartPosition();
    while (pos != NULL)
    {
        m_mapSessions.GetNextAssoc(pos, hTargetKeyItem, pTargetValueWnd);
        if (pTargetValueWnd && pTargetValueWnd->GetSafeHwnd() && pTargetValueWnd->GetDlgCtrlID() == (int)nID)
        {
            bFound = true;
            break;
        }
    }

    if (bFound && pTargetValueWnd != nullptr)
    {
        // 2. Revert the tree control state flag back to default (clears color highlights)
        if (hTargetKeyItem != NULL)
        {
            m_wndTree.SetItemData(hTargetKeyItem, 0);
        }

        // 3. Purge the track record from your map
        m_mapSessions.RemoveKey(hTargetKeyItem);

        // 4. Update the active view manager focus pointer if the closed session was on screen
        if (pTargetValueWnd == m_pActiveRdpWnd)
        {
            m_pActiveRdpWnd = nullptr;

            // Fallback focus automatically to the first available remaining background connection
            if (!m_mapSessions.IsEmpty())
            {
                HTREEITEM hFallbackKey = NULL;
                CWnd* pFallbackWnd = nullptr;
                POSITION fallbackPos = m_mapSessions.GetStartPosition();
                m_mapSessions.GetNextAssoc(fallbackPos, hFallbackKey, pFallbackWnd);

                if (pFallbackWnd && hFallbackKey)
                {
                    // Trigger a layout calculation refresh to pull the fallback window into view
                    m_pActiveRdpWnd = pFallbackWnd;
                    CRect clientRect;
                    GetClientRect(&clientRect);
                    RearrangeControls(clientRect.Width(), clientRect.Height());

                    m_wndTree.SelectItem(hFallbackKey);
                    m_wndTree.EnsureVisible(hFallbackKey);
                }
            }
        }

        // 5. Terminate and delete the background memory allocation footprint cleanly
        pTargetValueWnd->DestroyWindow();
        delete pTargetValueWnd;

        // Force tree canvas refresh to instantly clear out connection highlights
        m_wndTree.Invalidate();
    }
}

void CRdpSsmConnManagerDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == IDC_LAUNCH_DELAY_TIMER)
    {
        this->KillTimer(IDC_LAUNCH_DELAY_TIMER);
        printf("[DELAY-TIMER] UI Thread loop is clean. Launching RDP network channel...\n");

        CWnd* pTargetWnd = nullptr;
        if (m_mapSessions.Lookup(m_hPendingSelectedNode, pTargetWnd) && pTargetWnd != nullptr)
        {

            this->KillTimer(IDC_RDP_CTRL_START);
            this->SetTimer(IDC_RDP_CTRL_START, 1000, nullptr);

            InitializeRdpControl(pTargetWnd->GetSafeHwnd(), m_rectPendingZone, m_nPendingSelectedPort);
        }
        return;
    }

    if (nIDEvent == IDC_RDP_CTRL_START)
    {
        if (m_mapSessions.IsEmpty())
        {
            this->KillTimer(IDC_RDP_CTRL_START);
            return;
        }

        HTREEITEM hKeyItem = NULL;
        CWnd* pWnd = nullptr;
        POSITION pos = m_mapSessions.GetStartPosition();

        while (pos != nullptr)
        {
            m_mapSessions.GetNextAssoc(pos, hKeyItem, pWnd);

            if (pWnd && ::IsWindow(pWnd->GetSafeHwnd()))
            {
                LPUNKNOWN pUnk = pWnd->GetControlUnknown();
                if (pUnk == nullptr) continue;

                CComDispatchDriver rdpDisp(pUnk);
                CComVariant varConnected;

                if (FAILED(rdpDisp.GetPropertyByName(L"Connected", &varConnected))) continue;

                long nConnectedState = -1;
                if (SUCCEEDED(VariantChangeType(&varConnected, &varConnected, 0, VT_I4)))
                {
                    nConnectedState = varConnected.lVal;
                }

                // Connection is established and stable! Release safety layout lock flag

                if (nConnectedState == 0)
                {
                    // --- CONNECTING SAFETY PROTECTION ---
                    // If the unmanaged local loopback socket is still handshaking,
                    // skip running eviction tasks so it doesn't drop the connection too early!

                    // Explicitly evict ONLY if the connection was fully stable, but then turned 0 (User Logoff)
                    printf("[RDP-TIMER] !!! DISCONNECT DETECTED !!! Server reports closed. Evicting map entry...\n");
                    HandleRemoteLogoff(hKeyItem, pWnd);
                    return;
                }
            }
        }
    }

    CDialogEx::OnTimer(nIDEvent);
}



void CRdpSsmConnManagerDlg::HandleRemoteLogoff(HTREEITEM hDeadKey, CWnd* pDeadWnd)
{
    if (!pDeadWnd) return;

    // 1. Clear text color / status highlight data from the tree node layout
    m_wndTree.SetItemData(hDeadKey, 0);

    // 2. Erase the tracking record association from your CMap
    m_mapSessions.RemoveKey(hDeadKey);

    // 3. Clear or swap active focus tracking window parameters
    if (pDeadWnd == m_pActiveRdpWnd)
    {
        m_pActiveRdpWnd = nullptr; // Reset to empty baseline

        // If there are other background connections remaining, bring the next one up
        if (!m_mapSessions.IsEmpty())
        {
            HTREEITEM hFallbackKey = NULL;
            CWnd* pFallbackWnd = nullptr;
            POSITION fallbackPos = m_mapSessions.GetStartPosition();
            m_mapSessions.GetNextAssoc(fallbackPos, hFallbackKey, pFallbackWnd);

            if (pFallbackWnd)
            {
                m_pActiveRdpWnd = pFallbackWnd;

                CRect clientRect;
                GetClientRect(&clientRect);
                RearrangeControls(clientRect.Width(), clientRect.Height());

                m_wndTree.SelectItem(hFallbackKey);
                m_wndTree.EnsureVisible(hFallbackKey);
            }
        }
    }

    // 4. Safely terminate the underlying window and clean its heap memory allocations
    pDeadWnd->DestroyWindow();
    delete pDeadWnd;

    // 5. Force tree layout engine redraw to update colors instantly
    m_wndTree.Invalidate();
}

LRESULT CRdpSsmConnManagerDlg::OnPostInitializeRdp(WPARAM wParam, LPARAM lParam)
{
    HTREEITEM hTargetItem = (HTREEITEM)wParam;
    int nPort = (int)lParam;

    CWnd* pTargetWnd = nullptr;
    if (m_mapSessions.Lookup(hTargetItem, pTargetWnd) && pTargetWnd != nullptr)
    {
        CRect rectClient;
        GetClientRect(&rectClient);

        int nRdpLeft = 10 + m_nTreeWidth + m_nSplitterWidth;
        CRect rectVisibleZone(nRdpLeft, 10, rectClient.Width() - 10, rectClient.Height() - 20);

        printf("[ASYNC-THREAD] UI thread clear. Launching InitializeRdpControl safely...\n");

        // Handshake the advanced properties using baseline dimensions to ensure validation clears
        InitializeRdpControl(pTargetWnd->GetSafeHwnd(), rectVisibleZone, nPort);
    }
    return 0;
}

void CRdpSsmConnManagerDlg::OnNMClickTreeRdg(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0; // Default routing

    // Find exactly what node was clicked under the mouse cursor
    CPoint ptCursor;
    GetCursorPos(&ptCursor);
    m_wndTree.ScreenToClient(&ptCursor);

    UINT flags = 0;
    HTREEITEM hClickedItem = m_wndTree.HitTest(ptCursor, &flags);

    // Ensure they clicked on the actual item label or icon text
    if (hClickedItem != nullptr && (flags & (TVHT_ONITEMLABEL | TVHT_ONITEMICON)))
    {
        DWORD_PTR dwData = m_wndTree.GetItemData(hClickedItem);
        int nPort = (int)dwData;

        if (nPort > 0)
        {
            CWnd* pTargetRdpWnd = nullptr;

            // VIEW SWITCH CHECK: If this connection is already active in background memory...
            if (m_mapSessions.Lookup(hClickedItem, pTargetRdpWnd) && pTargetRdpWnd != nullptr)
            {
                if (pTargetRdpWnd != m_pActiveRdpWnd)
                {
                    printf("[SINGLE-CLICK] Session found open. Swapping viewport to active node...\n");

                    CRect rectClient;
                    GetClientRect(&rectClient);

                    // 1. Teleport previous window off-screen to keep its state alive
                    if (m_pActiveRdpWnd != nullptr)
                    {
                        m_pActiveRdpWnd->SetWindowPos(NULL, -32000, -32000, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
                    }

                    // 2. Point tracker focus to our target session and hide tree overlay
                    m_pActiveRdpWnd = pTargetRdpWnd;
                    m_bTreeVisible = false;

                    // 3. Layout engine shifts the tree away and renders target full screen instantly
                    RearrangeControls(rectClient.Width(), rectClient.Height());
                }
                else
                {
                    // If they single-clicked the session they are already looking at, just tuck the tree overlay away
                    m_bTreeVisible = false;
                    CRect rectClient;
                    GetClientRect(&rectClient);
                    RearrangeControls(rectClient.Width(), rectClient.Height());
                }
            }
        }
    }
}

void CRdpSsmConnManagerDlg::OnNMDblclkTreeRdg(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0; // Default routing

    CPoint ptCursor;
    GetCursorPos(&ptCursor);
    m_wndTree.ScreenToClient(&ptCursor);

    UINT flags = 0;
    HTREEITEM hClickedItem = m_wndTree.HitTest(ptCursor, &flags);

    if (hClickedItem != nullptr && (flags & (TVHT_ONITEMLABEL | TVHT_ONITEMICON)))
    {
        DWORD_PTR dwData = m_wndTree.GetItemData(hClickedItem);
        int nSelectedPort = (int)dwData;

        if (nSelectedPort > 0)
        {
            CWnd* pTargetRdpWnd = nullptr;

            // LAUNCH PROTECTION: Only open if a background channel does not exist yet!
            if (!m_mapSessions.Lookup(hClickedItem, pTargetRdpWnd))
            {
                printf("[DOUBLE-CLICK] Spawning fresh isolated session instance container...\n");

                CRect rectClient;
                GetClientRect(&rectClient);

                // Hide tree panel overlay layout bounds right at birth
                m_bTreeVisible = false;
                RearrangeControls(rectClient.Width(), rectClient.Height());

                // Calculate full wall-to-wall canvas dimensions
                int nRdpLeft = 5;
                int nRdpWidth = rectClient.Width() - nRdpLeft - 10;
                int nRdpHeight = rectClient.Height() - 20;

                CRect rectFullWallToWall(nRdpLeft, 10, nRdpLeft + nRdpWidth, 10 + nRdpHeight);

                CWnd* pNewWnd = new CWnd();

                BOOL bCreated = pNewWnd->CreateControl(L"MsTscAx.MsTscAx", nullptr,
                    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                    rectFullWallToWall, this, 2000 + nSelectedPort);

                if (bCreated)
                {
                    printf("[LAUNCHER] CreateControl completed successfully wall-to-wall.\n");

                    m_mapSessions.SetAt(hClickedItem, pNewWnd);
                    m_pActiveRdpWnd = pNewWnd;

                    // Cache structural properties for our safe delayed initialization thread
                    m_hPendingSelectedNode = hClickedItem;
                    m_nPendingSelectedPort = nSelectedPort;
                    m_rectPendingZone = rectFullWallToWall;

                    // Fire up the 10ms async delay worker timer
                    this->SetTimer(IDC_LAUNCH_DELAY_TIMER, 10, nullptr);
                }
                else
                {
                    printf("[LAUNCHER] ERROR: CreateControl failed!\n");
                    delete pNewWnd;
                }
            }
            else
            {
                printf("[DOUBLE-CLICK] Ignored. Session already open. Use Single-Click to look at it.\n");
            }
        }
    }
}

LRESULT CRdpSsmConnManagerDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_LBUTTONDOWN)
    {
        // Convert the raw lParam window coordinates into a clean CPoint stack object
        CPoint point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

        // If the tree is visible and the cursor coordinates hit right on its sizing border edge (+/- 5px)
        if (m_bTreeVisible && (point.x >= m_nTreeWidth - 5 && point.x <= m_nTreeWidth + 5))
        {
            printf("[WINDOWPROC] Intercepted edge click. Booting up drag matrix engine...\n");

            m_bIsDraggingEdge = true;
            SetCapture(); // Freeze mouse routing focus down to this main dialog shell frame

            return 1; // Return 1 to tell Windows the message was handled, blocking it from the tree!
        }
    }

    return CDialogEx::WindowProc(message, wParam, lParam);
}
