import Scene.Raytracing;
import Rendering.Materials.TexLODHelpers;
import Rendering.Materials.TexLODTypes;

RWTexture2D<float4> gStochasticDepthOut;
Texture2D<uint> aoMaskIn;

#ifndef NUM_SAMPLES
#define NUM_SAMPLES 4
#endif

#ifndef RAY_CONE_SPREAD
#define RAY_CONE_SPREAD 0.f
#endif

#include "Common.slangh"

struct RayData // cannot be compressed to half floats => no diff in rendering time + insufficient visual quality
{
    float depths[NUM_SAMPLES];
    uint count; // number of processed samples
};

//ExplicitLodTextureSampler computeLod(VertexData v, float3 rayDir) { return ExplicitLodTextureSampler(0.0); }

ExplicitRayConesLodTextureSampler computeLod(VertexData v, float3 rayDir)
{
    RayCone rc = RayCone(0.0, RAY_CONE_SPREAD);
    rc = rc.propagateDistance(RayTCurrent());
    float lambda = rc.computeLOD(v.coneTexLODValue, rayDir, v.faceNormalW);
    return ExplicitRayConesLodTextureSampler(lambda);
}

[shader("anyhit")]
void anyHit(inout RayData rayData, BuiltInTriangleIntersectionAttributes attribs)
{
    bool commit = algorithm(rayData.depths, rayData.count, attribs.barycentrics, RayTCurrent(), getGeometryInstanceID(), PrimitiveIndex(), WorldRayDirection());
    if (!commit)
    {
        IgnoreHit();
    }
}

[shader("miss")]
void miss(inout RayData rayData)
{
}

[shader("closesthit")]
void closestHit(inout RayData rayData, BuiltInTriangleIntersectionAttributes attribs)
{
}

[shader("raygeneration")]
void rayGen()
{
    uint2 svPos = DispatchRaysIndex().xy;

    uint mask = aoMaskIn[svPos] & 0xFF;

    RayDesc ray = initRayDesc(svPos, DispatchRaysDimensions().xy);

    RayData rayData;
    rayData.count = 0;
    for (uint i = 0; i < NUM_SAMPLES; ++i)
    {
        rayData.depths[i] = DEFAULT_DEPTH;
    }

    TraceRay(
        gScene.rtAccel,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_NON_OPAQUE | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
        0xff /* instanceInclusionMask */, 0 /* hitIdx */, 1 /*ray type count*/, 0 /* missIdx */, ray, rayData
    );

    float4 data;
    [unroll(NUM_SAMPLES)] for (uint i = 0; i < NUM_SAMPLES; ++i)
    {
        data[i] = rayData.depths[i];
    }
    gStochasticDepthOut[svPos] = data;
}
