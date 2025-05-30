/***************************************************************************
 # Copyright (c) 2015-23, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/

/**
 * This file contains various hash functions and other utilities
 * for pseudorandom number generation.
 */

/**
 * 32-bit (non-cryptographic) hash function by Robert Jenkins.
 * This is a perfect hash function (no collisions).
 * See https://gist.github.com/badboy/6267743.
 */
uint jenkinsHash(uint a)
{
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

/**
 * Generates a pair of 32-bit pseudorandom numbers based on a pair of 32-bit values.
 *
 * The code uses a 64-bit block cipher, the Tiny Encryption Algorithm (TEA) by Wheeler et al., 1994.
 * The 128-bit key is fixed and adapted from here: https://www.ibiblio.org/e-notes/webcl/mc.htm.
 * This function can be useful for seeding other pseudorandom number generators.
 *
 * @param[in] v0 The first value (low dword of the block).
 * @param[in] v1 The second value (high dword of the block).
 * @param[in] iterations Number of iterations (the authors recommend 16 at a minimum).
 * @return Two pseudorandom numbers (the block cipher of (v0,v1)).
 */
uint2 blockCipherTEA(uint v0, uint v1, uint iterations = 16)
{
    uint sum = 0;
    const uint delta = 0x9e3779b9;
    const uint k[4] = { 0xa341316c, 0xc8013ea4, 0xad90777d, 0x7e95761e }; // 128-bit key.
    for (uint i = 0; i < iterations; i++)
    {
        sum += delta;
        v0 += ((v1 << 4) + k[0]) ^ (v1 + sum) ^ ((v1 >> 5) + k[1]);
        v1 += ((v0 << 4) + k[2]) ^ (v0 + sum) ^ ((v0 >> 5) + k[3]);
    }
    return uint2(v0, v1);
}

#define HASH_TYPE_DEMOSCENE 0
#define HASH_TYPE_PCG 1
#define HASH_TYPE_FLOAT 2

#ifndef HASH_TYPE
#define HASH_TYPE 0
#endif

#define UINT_MAX 4294967295.f

float pcgHash(uint seed)
{
    seed = seed * 747796405u + 2891336453u;
    seed = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
    seed = (seed >> 22u) ^ seed;
    
    // Convert uint32 to normalized float [0,1]
    return frac(float(seed) / UINT_MAX) * 2.f - 1.f;
}

float pcgHash2(float2 seed)
{
    uint h = uint(seed.x * 1664525u + seed.y * 1013904223u);

    return pcgHash(h);
}

// hash function from "improved alpha testing using hashed sampling"
float hash(float2 v)
{
#if HASH_TYPE == HASH_TYPE_PCG
    return pcgHash2(v);
#elif HASH_TYPE == HASH_TYPE_FLOAT // Demoscene hash
    return floatHash(v);
#else
    return frac(1.0e4 * sin(17.0 * v.x + 0.1 * v.y) * (0.1 + abs(sin(13.0 * v.y + v.x))));
#endif
}

// 3d hash function from "improved alpha testing using hashed sampling"
float hash3d(float3 v)
{
    return hash(float2(hash(v.xy), v.z));
}

// 4d hash function based on the 3d hash function
float hash4d(float4 v)
{
    return hash(float2(hash3d(v.xyz), v.w));
}

// Random Normal distribution
float hashNormalDistribution(float randomNumber)
{
    float theta = 2. * 3.14155926 * randomNumber;
    float rho = sqrt(-2. * log(hash(randomNumber)));
    return rho * cos(theta);
}

// Random Direction
float3 randomDirection(float randomNumber)
{
    float x = hashNormalDistribution(randomNumber);
    float y = hashNormalDistribution(x);
    float z = hashNormalDistribution(y);

    return float3(x, y, z);
}

float3 randomPointInSphere(float radius, float randomNumber)
{
    float3 dir = randomDirection(randomNumber);
    return dir * radius * hash3d(dir);
}

float3 randomDirectionInHemisphere(float3 normal, float randomNumber)
{
    float3 dir = randomDirection(randomNumber);
    return dir * sign(dot(normal, dir));
}

float3 randomPointInHemisphere(float3 normal, float radius, float randomNumber)
{
    float3 dir = randomDirectionInHemisphere(normal, randomNumber);
    return dir * radius * hash3d(dir);
}
