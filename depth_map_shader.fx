#include "constants.hpp"

//cbuffer cbScene : register(b0)
//{
//    matrix ViewMtrx;
//    float4 CameraPos;
//    matrix ProjectionMtrx;
//};

//cbuffer cbSceneNode : register(b2)
//{
//    matrix WorldMtrx;
//    float4 MeshColor;
//};

cbuffer MatrixBuffer
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

//struct VS_INPUT
//{
//    float4 Pos : POSITION;
//    float3 Normal : NORMAL;
//    float4 Tangent : TANGENT;
//    float2 Tex : TEXCOORD0;
//};
//
//struct PS_INPUT
//{
//    float4 PosProj : SV_POSITION;
//    float4 PosWorld : TEXCOORD0;
//    float3 Normal : TEXCOORD1;
//    float4 Tangent : TEXCOORD2; // TODO: Semantics?
//    float2 Tex : TEXCOORD3;
//};

struct VertexInputType
{
    float4 position : POSITION;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 depthPosition : TEXTURE0;
};

//PS_INPUT VS(VS_INPUT input)
//{
//    PS_INPUT output = (PS_INPUT)0;
//
//    output.PosWorld = mul(input.Pos, WorldMtrx);
//
//    output.PosProj = mul(output.PosWorld, ViewMtrx);
//    output.PosProj = mul(output.PosProj, ProjectionMtrx);
//
//    output.Normal = mul(input.Normal, (float3x3)WorldMtrx);
//    output.Tangent = float4(mul(input.Tangent.xyz, (float3x3)WorldMtrx),
//                            input.Tangent.w);
//
//    output.Tex = input.Tex;
//
//    return output;
//}

PixelInputType DepthMapVertexShader(VertexInputType input)
{
    PixelInputType output;

    // Change the position vector to be 4 units for proper matrix calculations.
    input.position.w = 1.0f;

    // Calculate the position of the vertex against the world, view, and projection matrices.
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);

    // Store the position value in a second input value for depth value calculations.
    output.depthPosition = output.position;

    return output;
}

float4 DepthMapPixelShader(PixelInputType input) : SV_TARGET
{
	float depthValue;
	float4 color;

	// Get the depth value of the pixel by dividing the Z pixel depth by the homogeneous W coordinate.
	depthValue = input.depthPosition.z / input.depthPosition.w;

	// First 10% of the depth buffer color red.
	if(depthValue < 0.9f)
		color = float4(1.0, 0.0f, 0.0f, 1.0f);
	
	// The next 0.025% portion of the depth buffer color green.
	if(depthValue > 0.9f)
		color = float4(0.0, 1.0f, 0.0f, 1.0f);

	// The remainder of the depth buffer color blue.
	if(depthValue > 0.925f)
		color = float4(0.0, 0.0f, 1.0f, 1.0f);

	return color;
}
