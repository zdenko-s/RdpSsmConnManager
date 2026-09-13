#pragma once
#include <afxstr.h>
#include "IAwsSsmTunnel.h"

// Structural blueprint to track live AWS metadata per server leaf node
struct AwsServerTargetInfo
{
    CString sInstanceId;
    CString sRegion;
    int     nLocalPort = 0;
};

class CMockAwsSsmTunnelManager : public IAwsSsmTunnel
{
public:
    CMockAwsSsmTunnelManager() = default;
    ~CMockAwsSsmTunnelManager() = default;

    // Disallow copying to keep handle management safe
    CMockAwsSsmTunnelManager(const CMockAwsSsmTunnelManager&) = delete;
    CMockAwsSsmTunnelManager& operator=(const CMockAwsSsmTunnelManager&) = delete;

    // Core method to execute the mock handshake sequence
    //bool StartSdkSsmTunnel(const CString& sRegion, const CString& sInstanceId, int& nOutLocalPort);
    virtual bool Open(const std::wstring& awsProfile,
        const std::wstring& awsRegion,
        const std::wstring& instanceId,
        int& outLocalPort) override;

};
