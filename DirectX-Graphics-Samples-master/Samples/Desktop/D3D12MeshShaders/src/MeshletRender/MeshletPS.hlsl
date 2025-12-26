//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

struct Constants
{
    float4x4 World;
    float4x4 WorldView;
    float4x4 WorldViewProj;
    uint     DrawMeshlets;
    uint     UseTexture;
};

struct VertexOut
{
    float4 PositionHS   : SV_Position;
    float3 PositionVS   : POSITION0;
    float3 Normal       : NORMAL0;
    float2 TexCoord     : TEXCOORD0;
    uint   MeshletIndex : COLOR0;
};

ConstantBuffer<Constants> Globals : register(b0);

// Textures and sampler
Texture2D<float4> DiffuseTexture : register(t4);
Texture2D<float4> NormalTexture  : register(t5);
SamplerState      LinearSampler  : register(s0);

float4 main(VertexOut input) : SV_TARGET
{
    float ambientIntensity = 0.15;
    float3 lightDir = normalize(float3(0, 0.3, 1));

    float3 normal = normalize(input.Normal);
    
    float3 diffuseColor;
    if (Globals.UseTexture)
    {
        // Sample diffuse texture using UV from model (flip V)
        float2 uv = float2(input.TexCoord.x, 1.0 - input.TexCoord.y);
        diffuseColor = DiffuseTexture.Sample(LinearSampler, uv).rgb;
    }
    else
    {
        // Show meshlet colors when texture is disabled
        uint meshletIndex = input.MeshletIndex;
        diffuseColor = float3(
            float(meshletIndex & 1),
            float(meshletIndex & 3) / 4,
            float(meshletIndex & 7) / 8);
    }
    
    float shininess = 32.0;

    // Blinn-Phong shading
    float cosAngle = saturate(dot(normal, lightDir));
    float3 viewDir = -normalize(input.PositionVS);
    float3 halfAngle = normalize(lightDir + viewDir);

    float blinnTerm = saturate(dot(normal, halfAngle));
    blinnTerm = cosAngle != 0.0 ? blinnTerm : 0.0;
    blinnTerm = pow(blinnTerm, shininess);

    float3 finalColor = (cosAngle + blinnTerm * 0.3 + ambientIntensity) * diffuseColor;

    return float4(finalColor, 1);
}
