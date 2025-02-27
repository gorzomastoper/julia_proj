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

cbuffer scene_cbuffer : register(b0)
{
	float4 offset;
	float4 mouse_pos;
	float4x4 camera;
	float4 padding[10]; // For 256-byte size alignment
};

// cbuffer simulation_properties : register(b1) {
// 	float particle_count;
// 	float smoothing_radius;
// 	float pressure;
// 	float max_velocity;
// 	float4 color_a;
// 	float4 color_b;
// 	float4 color_c;
// 	float4 padding_[12]; // For 256-byte size alignment
// };

struct PSInput
{
    float4 position : SV_POSITION;
	float4 color : COLOR;
    float2 uv : TEXCOORD;
};

// Texture2D g_texture : register(t0);
// SamplerState g_sampler : register(s0);
RWStructuredBuffer <float4x4> model_matrices_for_objects : register(u6);
// RWBuffer <float2> velocity_buffer : register(u1); 

PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD, uint instanceID : SV_InstanceID)
{
    PSInput result;
	float4x4 MVP = mul(model_matrices_for_objects[instanceID], camera);
	result.position = mul(position, MVP);
	result.position.xyz /= -result.position.w;

	// float speed = length(velocity_buffer[instanceID]);
	// float speed_t = saturate(speed / max_velocity);
	// result.color = lerp(color_a, lerp(color_b, color_c, speed_t), speed_t);
	result.color = float4(1.0f, 1.0f, 0.0f, 1.0f);
    result.uv = uv;
    return result;
}

float4 circle(float2 uv, float2 pos, float rad, float3 color) {
	float d = length(pos - uv) - rad;
	float t = clamp(d, 0.0, 1.0);
	return float4(color, 1.0 - t);
}

float4 PSMain(PSInput input) : SV_TARGET
{
	// float2 ndc = input.position.xy;
	// float2 screen_coord = (ndc + 1.0) * 0.5;
	// float2 pixel_coord = (screen_coord / offset.zw);
	// float4 layer1 = float4(0.0, 0.0, 0.0, 0.0);
	// float4 layer2 = circle(input.uv, float2(0.5,0.5), 0.04, float3(1.0, 0.0, 0.0));
	// return lerp(layer1, layer2, max(0.0, layer2.a));
	// float2 centre_offset = (input.uv.xy - 0.5) * 2.0;
	// float sqr_distance = dot(centre_offset, centre_offset);
	// float delta = fwidth(sqrt(sqr_distance));
	// float alpha = 1.0 - smoothstep(1.0 - delta, 1.0 + delta, sqr_distance);
	
	return float4(input.color.xyz, 1.0f);
	// return float4(input.color.xyz, alpha);
    //return g_texture.Sample(g_sampler, input.uv);
}
