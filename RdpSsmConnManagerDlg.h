#pragma once
#include <afxtempl.h>

#include "AwsSsmTunnelManager.h" 

#define IDM_SWITCH_SESSIONS_START   40100
#define IDM_SWITCH_SESSIONS_END     40120  // Supports up to 20 open sessions

class CRdpSsmConnManagerDlg : public CDialogEx
{
public:
    CRdpSsmConnManagerDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_RDPSSMCONNMANAGER_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTvnSelchangedTreeRdg(NMHDR* pNMHDR, LRESULT* pResult);
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;

    // Mouse capture events for tracking the custom divider line
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    BOOL OnCommand(WPARAM wParam, LPARAM lParam);
    //afx_msg void OnNcRButtonUp(UINT nHitTest, CPoint point);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    // Add the handler signature block for RDP event ID 4 (OnDisconnected)
    afx_msg void OnRdpDisconnected(UINT nID, long discReason);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg LRESULT OnPostInitializeRdp(WPARAM wParam, LPARAM lParam);
    afx_msg void OnNMClickTreeRdg(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMDblclkTreeRdg(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDestroy();
    void HandleRemoteLogoff(HTREEITEM hDeadKey, CWnd* pDeadWnd);

    DECLARE_MESSAGE_MAP()

    HICON m_hIcon; // Icon handle storage wrapper
private:
    CTreeCtrl m_wndTree;
    CString   m_strRdgPath;
    CMap<HTREEITEM, HTREEITEM, CWnd*, CWnd*> m_mapSessions;
    CWnd* m_pActiveRdpWnd;

    // NEW: Layout Resizing Variables
    int  m_nTreeWidth;       // Current dynamic width of the left tree panel
    BOOL m_bDraggingSplitter; // Tracks if user is actively dragging the divider line
    int  m_nSplitterWidth;   // Visual padding gap size for the divider zone

    void ParseCommandLineArgs();
    void LoadRdgFile(const CString& strPath);
    CWnd* InitializeRdpControl(HWND hwndParent, const CRect& rect, int port);
    void RearrangeControls(int cx, int cy); // Helper to consolidate layout positions
    void SyncTreeSelection(const CString& serverName);

    bool      m_bTreeVisible = true;
    HTREEITEM m_hPendingSelectedNode = NULL;
    int       m_nPendingSelectedPort = 0;
    CRect     m_rectPendingZone;

    bool m_bIsDraggingEdge = false; // Tracks if the user is currently resizing the tree
    int  m_nMinTreeWidth = 150;     // Safety lower limit for tree size
    int  m_nMaxTreeWidth = 600;     // Safety upper limit for tree size

    enum { IDC_LAUNCH_DELAY_TIMER = 32500 }; // Unique ID for our 10ms kickstarter

	// AWS SSM Tunnel Manager instance for handling session tunnels
    CAwsSsmTunnelManager m_awsTunnelMgr; // Decoupled manager module instance

    // Recursive cleanup helper for application shutdown memory tracking
    void DeleteTreeItemDataRecursive(HTREEITEM hItem);
};
