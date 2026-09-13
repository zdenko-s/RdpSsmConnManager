#pragma once
#include "IAwsSsmTunnel.h"
#include <windows.h>

class AwsExeSsmTunnel : public IAwsSsmTunnel {
public:
    AwsExeSsmTunnel();
    virtual ~AwsExeSsmTunnel() override;
    // Checks if aws.exe can be located anywhere on the application or system paths
    bool IsAwsCliInstalled();

    virtual bool Open(const std::wstring& awsProfile,
        const std::wstring& awsRegion,
        const std::wstring& instanceId,
        int& outLocalPort) override;

private:
    int GetVacantLocalPort();

    HANDLE m_hJob;
    HANDLE m_hProcess;
    HANDLE m_hThread;
};
