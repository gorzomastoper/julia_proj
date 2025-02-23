#define PI 3.14159265f
#define TAU 6.28318530f

#define SPAN_MAX (32.0)
#define REDUCE_MIN (1.0 / 64.0)
#define REDUCE_MUL (1.0 / 2.0)


static float2 u_texel;

RWTexture2D<float4> buffer : register(u0);

float4 fxaa(uint2 uv, uint2 res) {
	// if (uv.x == 0) return buffer[uv];
	// if (uv.y == 0) return buffer[uv];

	float4 center 	= buffer[uv];
	float4 left 	= buffer[uv + int2(-1, 0)];
	float4 right 	= buffer[uv + int2(1, 0)];
	float4 top 		= buffer[uv + int2(0, -1)];
	float4 bottom 	= buffer[uv + int2(0, 1)];

	// Luma coeff
	float3 luma = float3(0.299f, 0.587f, 0.114f);

	float lumaCC = dot(center.rgb,luma);
	float luma00 = dot(left.rgb,luma);
	float luma10 = dot(right.rgb,luma);
	float luma01 = dot(top.rgb,luma);
	float luma11 = dot(bottom.rgb,luma);

	float edge_H = abs(luma00 - luma10);
	float edge_V = abs(luma01 - luma11);
	float edge = max(edge_H, edge_V);

	float3 blend = center.rgb;

	if(edge > REDUCE_MIN) {
		blend = (left.rgb + right.rgb + top.rgb + bottom.rgb) * REDUCE_MUL;
	}

	return float4(blend, 1.0f);
}

// float4 fxaa_fast(float2 uv, uint2 res) {
//     float2 coord = uv;
    
//     //Side divider
//     float side = uv.x - (float)res.x / 2.;

//     //Distance function
//     float dis = length(coord)-48.;
//     //SDF derivative
//     float2 der = float2(ddx(dis), ddy(dis));
//     //Derivative width
//     float wid = (side>0.)? length(der) : fwidth(dis);
//     //Anti-alias value
// 	float val = dis/wid/AA;
//     float aa = clamp(val, 0.0, 1.0f);
    
//     //Divider line
//     float edge = min(side*side/8.,1.);
// 	return lerp(buffer[uv], buffer[uv+1], aa);

// }

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint2 res;
	buffer.GetDimensions(res.x, res.y);
	u_texel.x = 1.0f / (float)res.x;
	u_texel.y = 1.0f / (float)res.y;
	buffer[dispatchThreadID.xy] = fxaa(dispatchThreadID.xy, res);
	// buffer[dispatchThreadID.xy] = fxaa_fast(dispatchThreadID.xy, res);
	// buffer[dispatchThreadID.xy] = buffer[dispatchThreadID.xy];
}