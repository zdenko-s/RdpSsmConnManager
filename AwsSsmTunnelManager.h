#pragma once
#include <afxstr.h>

// Structural blueprint to track live AWS metadata per server leaf node
struct AwsServerTargetInfo
{
    CString sInstanceId;
    CString sRegion;
    int     nLocalPort = 0;
};

class CAwsSsmTunnelManager
{
public:
    CAwsSsmTunnelManager() = default;
    ~CAwsSsmTunnelManager() = default;

    // Disallow copying to keep handle management safe
    CAwsSsmTunnelManager(const CAwsSsmTunnelManager&) = delete;
    CAwsSsmTunnelManager& operator=(const CAwsSsmTunnelManager&) = delete;

    // Core method to execute the mock handshake sequence
    bool StartSdkSsmTunnel(const CString& sRegion, const CString& sInstanceId, int& nOutLocalPort);
};
