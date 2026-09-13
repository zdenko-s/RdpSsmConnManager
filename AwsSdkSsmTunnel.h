#pragma once
#include "IAwsSsmTunnel.h"
#include <windows.h>
#include <memory>

// Forward declarations to avoid exposing heavy AWS headers to your UI/MFC layers
namespace Aws {
    namespace SSM {
        class SSMClient;
    }
}

class AwsSdkSsmTunnel : public IAwsSsmTunnel {
public:
    AwsSdkSsmTunnel();
    virtual ~AwsSdkSsmTunnel() override;

    virtual bool Open(const std::wstring& awsProfile,
        const std::wstring& awsRegion,
        const std::wstring& instanceId,
        int& outLocalPort) override;

private:
    int GetVacantLocalPort();
    bool InitializeAwsSdk(const std::wstring& profile, const std::wstring& region);
    void ShutdownAwsSdk();

    // Worker thread function that loops local TCP sockets data directly into the AWS WebSocket
    static DWORD WINAPI TunnelWorkerThread(LPVOID lpParam);

    // Private properties tracking SDK and listening loop state
    Aws::SSM::SSMClient* m_ssmClient;
    HANDLE               m_hWorkerThread;
    SOCKET               m_listenSocket;
    bool                 m_bTerminateSignal;
};
