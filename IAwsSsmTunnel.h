#pragma once
#include <string>

class IAwsSsmTunnel {
public:
    virtual ~IAwsSsmTunnel() = default;

    // Establishes the tunnel and outputs the dynamically allocated local port.
    // Returns true if successful, false otherwise.
    virtual bool Open(const std::wstring& awsProfile,
        const std::wstring& awsRegion,
        const std::wstring& instanceId,
        int& outLocalPort) = 0;
};
