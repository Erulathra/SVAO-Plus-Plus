Texture2D gColorIn;
Texture2D gLinearDepthIn;

RWTexture2D<unorm float> gColorOut;

SamplerState gSampler;

#ifndef DIRECTION
#define DIRECTION float2(1.f, 0.f)
#endif // DIRECTION

#ifndef KERNEL_RADIUS
#define KERNEL_RADIUS 4
#endif // KERNEL_RADIUS

#ifndef INV_RESOLUTION
#define INV_RESOLUTION 1.f / float2(1920.f, 1080.f)
#endif // INV_RESOLUTION

#ifndef BETTER_SLOPE
#define BETTER_SLOPE 1
#endif // BETTER_SLOPE

static float depthCache[2 * KERNEL_RADIUS + 1];
static float aoCache[2 * KERNEL_RADIUS + 1];

void AddSample(float sampleAO, float sampleDepth, float depthSlope, uint d, inout float AO, inout float weightSum)
{
    // weights from HBAO+
    const float BlurSigma = ((float)KERNEL_RADIUS+1.0) * 0.5;
    const float BlurFalloff = 1.0 / (2.0*BlurSigma*BlurSigma);
    const float Sharpness = 16.0;

    sampleDepth -= depthSlope * d;
    float dz = abs(sampleDepth - depthCache[KERNEL_RADIUS]) * Sharpness;
    // make dz relative to actual depth
    dz = dz * 12.0 / depthCache[KERNEL_RADIUS];

    float w = exp2(-float(d * d)*BlurFalloff - dz * dz);
    AO += w * sampleAO;
    weightSum += w;
}

void BlurDirection(inout float AO, inout float weightSum, float slope, int sign)
{
    float depthSlope = slope;
    [unroll]
    for(uint d = 1; d <= KERNEL_RADIUS; d++)
    {
        float sampleAO = aoCache[KERNEL_RADIUS + sign * d];
        float sampleDepth = depthCache[KERNEL_RADIUS + sign * d];

#if !BETTER_SLOPE
        if (d == 1) depthSlope = sampleDepth - depthCache[KERNEL_RADIUS];
#endif // !BETTER_SLOPE

        AddSample(sampleAO, sampleDepth, depthSlope, d, AO, weightSum);
    }
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint3 localId : SV_GroupThreadID)
{
    float2 gDUV = INV_RESOLUTION * DIRECTION;

    float2 uv = id.xy * INV_RESOLUTION;

    //fill depth cache
    [unroll]
    for(int d = -KERNEL_RADIUS; d <= KERNEL_RADIUS; d++)
    {
        float2 sampleUv = clamp(uv + d * gDUV, 0.f, 1.f); // manually clamp to guard band
        depthCache[KERNEL_RADIUS + d] = gLinearDepthIn.Sample(gSampler, sampleUv).x;
        aoCache[KERNEL_RADIUS + d] = gColorIn.Sample(gSampler, sampleUv).x;
    }

    // initial weight (of center sample)
    float AO = aoCache[KERNEL_RADIUS];
    float weightSum = 1.0;

    // compute ao and weights for positive and negative direction
    float slopeLeft = depthCache[KERNEL_RADIUS] - depthCache[KERNEL_RADIUS - 1];
    float slopeRight = depthCache[KERNEL_RADIUS + 1] - depthCache[KERNEL_RADIUS];
    float minSlope = abs(slopeLeft) < abs(slopeRight) ? slopeLeft : slopeRight;

    BlurDirection(AO, weightSum, minSlope, 1);
    BlurDirection(AO, weightSum, -minSlope, -1);

    gColorOut[id.xy] = AO / weightSum;
}
