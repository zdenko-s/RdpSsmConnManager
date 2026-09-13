#include "pch.h"

#include "AwsSdkSsmTunnel.h"
#include <winsock2.h>
#include <ws2tcpip.h>

// Note: Include your specific AWS SDK Headers here when installing Phase 2
// #include <aws/core/Aws.h>
// #include <aws/ssm/SSMClient.h>
// #include <aws/ssm/model/StartSessionRequest.h>

#pragma comment(lib, "Ws2_32.lib")

AwsSdkSsmTunnel::AwsSdkSsmTunnel()
    : m_ssmClient(nullptr), m_hWorkerThread(nullptr),
    m_listenSocket(INVALID_SOCKET), m_bTerminateSignal(false) {
}

AwsSdkSsmTunnel::~AwsSdkSsmTunnel() {
    // RAII Cleanup: Stop the listening thread and close raw sockets
    m_bTerminateSignal = true;

    if (m_listenSocket != INVALID_SOCKET) {
        ::closesocket(m_listenSocket);
    }

    if (m_hWorkerThread != nullptr) {
        // Wait gracefully for the background proxy thread to wind down
        ::WaitForSingleObject(m_hWorkerThread, 2000);
        ::CloseHandle(m_hWorkerThread);
    }

    ShutdownAwsSdk();
}

int AwsSdkSsmTunnel::GetVacantLocalPort() {
    WSADATA wsaData;
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 0;

    SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        ::WSACleanup();
        return 0;
    }

    sockaddr_in service;
    service.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &service.sin_addr);
    service.sin_port = 0;

    if (::bind(sock, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        ::closesocket(sock);
        ::WSACleanup();
        return 0;
    }

    sockaddr_in name;
    int nameLen = sizeof(name);
    int port = 0;
    if (::getsockname(sock, (SOCKADDR*)&name, &nameLen) != SOCKET_ERROR) {
        port = ntohs(name.sin_port);
    }

    // Note: Do not close the socket here like Phase 1! 
    // We hold onto this socket handle so your app actively binds and listens to it for incoming RDP traffic.
    m_listenSocket = sock;
    return port;
}

bool AwsSdkSsmTunnel::InitializeAwsSdk(const std::wstring& profile, const std::wstring& region) {
    // TODO: Implement your client configuration using the official AWS C++ SDK.
    // Convert wide strings to thin UTF-8 strings for the SDK configuration blocks.
    // m_ssmClient = new Aws::SSM::SSMClient(clientConfiguration);
    return true;
}

void AwsSdkSsmTunnel::ShutdownAwsSdk() {
    if (m_ssmClient != nullptr) {
        delete m_ssmClient;
        m_ssmClient = nullptr;
    }
    ::WSACleanup();
}

bool AwsSdkSsmTunnel::Open(const std::wstring& awsProfile,
    const std::wstring& awsRegion,
    const std::wstring& instanceId,
    int& outLocalPort) {

    // 1. Establish custom client variables
    if (!InitializeAwsSdk(awsProfile, awsRegion)) return false;

    // 2. Snag a unique listening port
    outLocalPort = GetVacantLocalPort();
    if (outLocalPort == 0) return false;

    /* TODO: Call SSM StartSession endpoint
    Aws::SSM::Model::StartSessionRequest request;
    request.SetTarget(...);
    request.SetDocumentName("AWS-StartPortForwardingSession");

    auto outcome = m_ssmClient->StartSession(request);
    if (!outcome.IsSuccess()) {
        return false;
    }

    auto sessionConnectionInfo = outcome.GetResult();
    std::string wsUrl = sessionConnectionInfo.GetStreamUrl();
    std::string token = sessionConnectionInfo.GetTokenValue();
    */

    // Start listening on the port
    if (::listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        return false;
    }

    // 3. Spin up background proxy worker thread
    m_hWorkerThread = ::CreateThread(nullptr, 0, TunnelWorkerThread, this, 0, nullptr);
    if (m_hWorkerThread == nullptr) return false;

    return true;
}

DWORD WINAPI AwsSdkSsmTunnel::TunnelWorkerThread(LPVOID lpParam) {
    auto* pThis = reinterpret_cast<AwsSdkSsmTunnel*>(lpParam);

    while (!pThis->m_bTerminateSignal) {
        // Accept incoming RDP client connections (e.g. from the RDP OCX control binding)
        SOCKET clientSocket = ::accept(pThis->m_listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) break;

        // TODO: Bridge raw byte streams from clientSocket into your chosen WebSocket client 
        // library to connect directly to the AWS StreamUrl/Token endpoints.
    }
    return 0;
}
