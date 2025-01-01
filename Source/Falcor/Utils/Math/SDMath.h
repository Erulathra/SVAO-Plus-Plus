#pragma once

namespace Falcor {

class SDMath {
public:
    static uint2 getStochMapSize(uint2 fullRes, bool includeGuard, uint32_t resolutionDivisor, int32_t guardBandSize = 512)
    {
        auto internalMapsRes = fullRes;
        if (resolutionDivisor > 1)
        {
            internalMapsRes.x = (internalMapsRes.x + resolutionDivisor - 1) / resolutionDivisor;
            internalMapsRes.y = (internalMapsRes.y + resolutionDivisor - 1) / resolutionDivisor;
        }

        if (includeGuard)
        {
            const uint32_t extraGuardBandSize = 2 * getExtraGuardBand(resolutionDivisor, guardBandSize); // expand internal size with the extra guard band from the SD-map
            internalMapsRes.x += extraGuardBandSize;
            internalMapsRes.y += extraGuardBandSize;
        }

        return internalMapsRes;
    }

    static int32_t getExtraGuardBand(uint32_t resolutionDivisor, int32_t guardBandSize = 512)
    {
        return guardBandSize / resolutionDivisor;
    }
};

} // Falcor
