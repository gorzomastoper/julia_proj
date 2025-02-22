#define PI 3.14159265f

float3 mod289(float3 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float2 mod289(float2 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float3 permute(float3 x) {
    return mod289((x * 34.0 + 1.0) * x);
}

float3 taylorInvSqrt(float3 r) {
    return 1.79284291400159 - 0.85373472095314 * r;
}

// output noise is in range [-1, 1]
float snoise(float2 v) {
    const float4 C = float4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
                            0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
                            -0.577350269189626, // -1.0 + 2.0 * C.x
                            0.024390243902439); // 1.0 / 41.0

    // First corner
    float2 i  = floor(v + dot(v, C.yy));
    float2 x0 = v -   i + dot(i, C.xx);

    // Other corners
    float2 i1;
    i1.x = step(x0.y, x0.x);
    i1.y = 1.0 - i1.x;

    // x1 = x0 - i1  + 1.0 * C.xx;
    // x2 = x0 - 1.0 + 2.0 * C.xx;
    float2 x1 = x0 + C.xx - i1;
    float2 x2 = x0 + C.zz;

    // Permutations
    i = mod289(i); // Avoid truncation effects in permutation
    float3 p =
      permute(permute(i.y + float3(0.0, i1.y, 1.0))
                    + i.x + float3(0.0, i1.x, 1.0));

    float3 m = max(0.5 - float3(dot(x0, x0), dot(x1, x1), dot(x2, x2)), 0.0);
    m = m * m;
    m = m * m;

    // Gradients: 41 points uniformly over a line, mapped onto a diamond.
    // The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)
    float3 x = 2.0 * frac(p * C.www) - 1.0;
    float3 h = abs(x) - 0.5;
    float3 ox = floor(x + 0.5);
    float3 a0 = x - ox;

    // Normalise gradients implicitly by scaling m
    m *= taylorInvSqrt(a0 * a0 + h * h);

    // Compute final noise value at P
    float3 g = float3(
        a0.x * x0.x + h.x * x0.y,
        a0.y * x1.x + h.y * x1.y,
        g.z = a0.z * x2.x + h.z * x2.y
    );
    return 130.0 * dot(m, g);
}

float fsnoise (float2 c) {return frac(sin(dot(c, float2(12.9898, 78.233))) * 43758.5453);}

float3x3 rotate3D(float angle, float3 axis){
    float3 a = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float r = 1.0 - c;
    return float3x3(
        a.x * a.x * r + c,
        a.y * a.x * r + a.z * s,
        a.z * a.x * r - a.y * s,
        a.x * a.y * r - a.z * s,
        a.y * a.y * r + c,
        a.z * a.y * r + a.x * s,
        a.x * a.z * r + a.y * s,
        a.y * a.z * r - a.x * s,
        a.z * a.z * r + c
    );
}

RWTexture2D<float4> buffer : register(u0);
cbuffer scene_cbuffer : register(b0)
{
	float4 offset;
	float4 padding[15]; // For 256-byte size alignment
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	int width;
	int height;
	buffer.GetDimensions(width, height);
	float t = offset.x;
	float4 o = float4(0,0,0,0);
	float2 r = float2(width, height);
	float3 FC = float3(width,height,1) - dispatchThreadID;
	float3 p = float3(0.0,0.0,3.0), v = float3(FC.xy*2.0f - r,r.y);
	for(float T,d,i,n=fsnoise(ceil(FC.y/r+t/0.3f)); i++ < 2e2;p -= v/length(v)*d)
	{
		T += d = min(length(max(abs(mul(p,rotate3D(n * t, tan(r.yxy+n*100)))) + n - 1.0f, -n*n*0.8f))-n*n, 1.0f-p.y);
		if(i > 1e2)
		{
			d += 1e-4;
			v = frac(cos(t + sqrt(FC.x*FC.y * float3(8,7,5)))*5e3);
			o.g=T;
		}
		else
		{
			T=0.0;
		}
	}

	buffer[dispatchThreadID.xy] = o;
}