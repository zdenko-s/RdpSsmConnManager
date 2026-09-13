#include "pch.h"

#include "AwsExeSsmTunnel.h"
#include <iostream>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

AwsExeSsmTunnel::AwsExeSsmTunnel()
    : m_hJob(nullptr), m_hProcess(nullptr), m_hThread(nullptr) {
}

AwsExeSsmTunnel::~AwsExeSsmTunnel() {
    // RAII Cleanup: The destructor handles terminating the process cleanly
    if (m_hJob != nullptr) {
        // Closing the unique Job Handle instantly kills the bound aws.exe process
        ::CloseHandle(m_hJob);
    }
    if (m_hProcess != nullptr)
        ::CloseHandle(m_hProcess);
    if (m_hThread != nullptr)
        ::CloseHandle(m_hThread);
}

int AwsExeSsmTunnel::GetVacantLocalPort() {
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
    service.sin_port = 0; // Windows auto-assigns an open port

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

    ::closesocket(sock);
    ::WSACleanup();
    return port;
}

bool AwsExeSsmTunnel::Open(const std::wstring& awsProfile,
    const std::wstring& awsRegion,
    const std::wstring& instanceId,
    int& outLocalPort) {

    // Safety check: Avoid allocating pipes/ports if the runtime executable is absent entirely
    if (!IsAwsCliInstalled()) {
        return false;
    }

    outLocalPort = GetVacantLocalPort();
    if (outLocalPort == 0)
        return false;

    std::wstring cmd = L"aws.exe ssm start-session"
        L" --target " + instanceId +
        L" --document-name AWS-StartPortForwardingSession" +
        L" --parameters \"portNumber=3389,localPortNumber=" + std::to_wstring(outLocalPort) + L"\"" +
        L" --region " + awsRegion +
        L" --profile " + awsProfile;

    HANDLE hChildStdOutRead = nullptr;
    HANDLE hChildStdOutWrite = nullptr;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    if (!::CreatePipe(&hChildStdOutRead, &hChildStdOutWrite, &saAttr, 0))
        return false;
    if (!::SetHandleInformation(hChildStdOutRead, HANDLE_FLAG_INHERIT, 0))
        return false;

    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    ::ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
    ::ZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));

    startupInfo.cb = sizeof(STARTUPINFO);
    startupInfo.hStdOutput = hChildStdOutWrite;
    startupInfo.hStdError = hChildStdOutWrite;
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;

    BOOL success = ::CreateProcess(
        nullptr, &cmd[0], nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW,
        nullptr, nullptr, &startupInfo, &processInfo
    );

    ::CloseHandle(hChildStdOutWrite); // Close write end so ReadFile EOF works properly

    if (!success) {
        ::CloseHandle(hChildStdOutRead);
        return false;
    }

    // Cache internal process references
    m_hProcess = processInfo.hProcess;
    m_hThread = processInfo.hThread;

    // Configure Job Object isolation for this instance
    m_hJob = ::CreateJobObject(nullptr, nullptr);
    if (m_hJob != nullptr) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        ::SetInformationJobObject(m_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        ::AssignProcessToJobObject(m_hJob, m_hProcess);
    }

    // Stream verification loop (Max 60 Seconds)
    bool tunnelReady = false;
    std::string accumulator = "";
    ULONGLONG startTime = ::GetTickCount64();
    const ULONGLONG timeoutMs = 60000;

    while (true) {
        if ((::GetTickCount64() - startTime) >= timeoutMs)
            break;

        DWORD exitCode;
        if (::GetExitCodeProcess(m_hProcess, &exitCode) && exitCode != STILL_ACTIVE)
            break;

        DWORD bytesAvailable = 0;
        if (::PeekNamedPipe(hChildStdOutRead, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0) {
            char buffer[128];
            DWORD bytesToRead = (bytesAvailable > sizeof(buffer) - 1) ? sizeof(buffer) - 1 : bytesAvailable;
            DWORD bytesRead = 0;

            if (::ReadFile(hChildStdOutRead, buffer, bytesToRead, &bytesRead, nullptr  ) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                accumulator += buffer;

                if (accumulator.find("Waiting for connections") != std::string::npos) {
                    tunnelReady = true;
                    break;
                }
            }
        }
        else {
            ::Sleep(50);
        }
    }

    ::CloseHandle(hChildStdOutRead);
    return tunnelReady;
}

bool AwsExeSsmTunnel::IsAwsCliInstalled() {
    wchar_t szBuffer[MAX_PATH] = { 0 };
    wchar_t* lpFilePart = nullptr;

    // SearchPathW searches app directory, working directory, system folders, and %PATH% environment values.
    // If it succeeds, it returns the length of the matching string. If it fails, it returns 0.
    DWORD dwLength = ::SearchPathW(
        nullptr,            // Null means look through standard Windows path order
        L"aws.exe",        // Executable filename target
        nullptr,            // No explicit extension modifier needed
        MAX_PATH,           // Buffer size allocation limits
        szBuffer,           // Pre-allocated array receiving full matching path string destination
        &lpFilePart         // Receives the memory address of the final filename component
    );

    return (dwLength > 0);
}
