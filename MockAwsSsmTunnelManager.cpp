#include "pch.h"
#include "MockAwsSsmTunnelManager.h"
#include <cstdio>

bool CMockAwsSsmTunnelManager::Open(const std::wstring& awsProfile,
        const std::wstring& awsRegion,
        const std::wstring& instanceId,
        int& outLocalPort)
{
    printf("[MOCK-SDK] >>> Handshake requested via isolated metadata file module <<<\n");
    wprintf(L"[MOCK-SDK] Target Extracted Region:   %s\n", awsRegion.c_str());
    wprintf(L"[MOCK-SDK] Target Extracted Instance: %s\n", instanceId.c_str());

    // HARDCODED PROFILE EVALUATION FALLBACKS FOR TESTING:
    // Sync these matching string tokens explicitly with active manual AWS CLI tunnel configurations
    if (instanceId == L"i-aaaaaaaaaaaaaaaaa")
    {
        outLocalPort = 34679; // Explicitly ties to manual AWS CLI terminal mapping string
        printf("[MOCK-SDK] Target Match localized! Assigning local loopback testing port: %d\n", outLocalPort);
    }
    else if (instanceId == L"i-bbbbbbbbbbbbbbbbb")
    {
        outLocalPort = 34680;
        printf("[MOCK-SDK] Target Match localized! Assigning local loopback testing port: %d\n", outLocalPort);
    }
    else
    {
        // Dynamic baseline generation if no custom string tokens matched
        outLocalPort = 34679;
        printf("[MOCK-SDK] WARNING: Unknown Instance ID. Defaulting onto baseline port map layout: %d\n", outLocalPort);
    }

    printf("[MOCK-SDK] SUCCESS: Returning true instantly. Handing thread routing to RDP canvas loop.\n");
    return true;
}
