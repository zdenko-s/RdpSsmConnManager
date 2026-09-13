#include "pch.h" // Replace with your project's precompiled header name if different
#include "AwsSsmTunnelManager.h"
#include <cstdio>

bool CAwsSsmTunnelManager::StartSdkSsmTunnel(const CString& sRegion, const CString& sInstanceId, int& nOutLocalPort)
{
    printf("[MOCK-SDK] >>> Handshake requested via isolated metadata file module <<<\n");
    printf("[MOCK-SDK] Target Extracted Region:   %s\n", (LPCSTR)CT2A(sRegion));
    printf("[MOCK-SDK] Target Extracted Instance: %s\n", (LPCSTR)CT2A(sInstanceId));

    // HARDCODED PROFILE EVALUATION FALLBACKS FOR TESTING:
    // Sync these matching string tokens explicitly with your active manual AWS CLI tunnel configurations
    if (sInstanceId.CompareNoCase(_T("i-aaaaaaaaaaaaaaaaa")) == 0)
    {
        nOutLocalPort = 34679; // Explicitly ties to your manual AWS CLI terminal mapping string
        printf("[MOCK-SDK] Target Match localized! Assigning local loopback testing port: %d\n", nOutLocalPort);
    }
    else if (sInstanceId.CompareNoCase(_T("i-bbbbbbbbbbbbbbbbb")) == 0)
    {
        nOutLocalPort = 34680;
        printf("[MOCK-SDK] Target Match localized! Assigning local loopback testing port: %d\n", nOutLocalPort);
    }
    else
    {
        // Dynamic baseline generation if no custom string tokens matched
        nOutLocalPort = 34679;
        printf("[MOCK-SDK] WARNING: Unknown Instance ID. Defaulting onto baseline port map layout: %d\n", nOutLocalPort);
    }

    printf("[MOCK-SDK] SUCCESS: Returning true instantly. Handing thread routing to RDP canvas loop.\n");
    return true;
}
