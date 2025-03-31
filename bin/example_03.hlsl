#define PI 3.14159265f
#define TAU 6.28318530f

#define _A 0.875f
#define _B 0.5f
#define _C 0.266f
#define _DOF_AMOUNT 1.0f
#define _DOF_FOCAL_DIST 0.426f
#define _PAUSED 1.0f
#define _D 0.127f

RWTexture2D<float4> buffer : register(u0);
RWBuffer<uint> hist_atomic : register(u1);
cbuffer scene_cbuffer : register(b0)
{
	float4 offset;
	float4 padding[15]; // For 256-byte size alignment
};

static uint seed;
static float2 R;
static float2 U;
static float2 muv;

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

float fsnoise (float2 c){return frac(sin(dot(c, float2(12.9898, 78.233))) * 43758.5453);}

float2x2 rot(float a){
	return float2x2(cos(a), -sin(a), sin(a), cos(a));
}

float3x3 rot_x(float a) {
	float2x2 r = rot(a); return float3x3(1, 0, 0 , 0, r[0][0], r[0][1], 0, r[1][0], r[1][1]);
}

float3x3 rot_y(float a) {
	float2x2 r = rot(a); return float3x3(r[0][0], 0.0, r[0][1] , 0.0, 1.0, 0.0, r[1][0], 0.0, r[1][1]);
}

float3x3 rot_z(float a) {
	float2x2 r = rot(a); return float3x3(r[0][0], r[0][1], 0.0, r[1][0], r[1][1], 0.0, 0.0, 0.0, 1.0);
}

uint hash_u(uint _a) {
	uint a = _a; 
	a ^= a >> 16;
	a *= 0x7feb352du;
	a ^= a >> 15;
	a *= 0x846ca68bu;
	a ^= a >> 16;
	return a;
}

float hash_f() {
	uint s = hash_u(seed);
	seed = s;
	return (float(s) / float(0xffffffffu));
}

float2 hash_v2() {return float2(hash_f(), hash_f());}
float3 hash_v3() {return float3(hash_f(), hash_f(), hash_f());}
float2 hash_v4() {return float4(hash_f(), hash_f(), hash_f(), hash_f());}

float sin_add(float a) { return sin(a) * 0.5 + 0.5; }

float2 sample_disk() {
	float2 r = hash_v2();
	return float2(sin(r.x * PI), cos(r.x * TAU)) / sqrt(r.x); // RETURN ALL PI HERE TO TAU!!!
}

#define COL_CNT 4
static float3 k_cols[COL_CNT] = {
	float3(1.0,1.0,1.0), float3(0.8f, 0.4f, 0.7f),
	float3(1.0,1.0,1.0) * 1.5, float3(1.0,1.0,1.0) * 1.5
};

float3 mix_cols(float _idx) {
	float idx = _idx % 1.0;
	int cols_idx = int(idx * float(COL_CNT));
	float fract_idx = frac(float(idx) * float(COL_CNT));
	fract_idx = smoothstep(0.0, 1.0, fract_idx);
	return lerp(k_cols[cols_idx], k_cols[(cols_idx + 1) % COL_CNT], fract_idx);
}

// float3 proj_particle(float3 _p) {
// 	float3 p = _p;
// 	float t = offset.x * 1.0;
// 	float dpp = dot(p,p) * sin(t + 12);

// 	p += float3(sin(t + 0.1) * 0.1, sin(t) * 0.1, sin(t) * 0.2) * 1.0;
// 	p *= 1.0 + sin(t * 0.9f + sin(t * 0.8)) * 0.05f;
// 	p /= dpp;

// 	p.z = _p.z;
// 	p.x /= (R.x/R.y);
// 	return p;
// }

float3 proj_particle(float3 _p) {
	float3 p = _p;
	float t = offset.x * 1.0;
	float dpp = dot(p,p);// * sin(t + 12);

	p += float3(sin(t + 0.1) * 0.1, sin(t) * 0.1, sin(t) * 0.2) * 1.0;
	p *= 1.0 + sin(t * 0.9f + sin(t * 0.8)) * 0.05f;
	p /= dpp;

	p.z = _p.z;
	p.x /= (R.x/R.y);
	return p;
}

float3 t_spherical(float3 p, float rad, float3 offs)
{
	return p / (dot(p,p)*rad + offs);
}

[numthreads(32, 1, 1)]
void Splat(uint3 id : SV_DispatchThreadID)
{
	uint2 Ru;
	buffer.GetDimensions(Ru.x, Ru.y);
	if(id.x >= Ru.x || id.y >= Ru.y) {return;}
	R = float2(Ru);
	U = float2(id.xy);
	muv = (float2(1.0, 1.0) - 0.5*R)/R.y;

	seed = hash_u(id.x + hash_u(Ru.x * id.y*200u)*20u + hash_u(id.x) * 250u + hash_u(id.z)*250u);
	seed = hash_u(seed);

	int iters = 60 - int(sin_add(offset.x)*30.0f);
	float t = offset.x + sin(offset.x*4.0) * 0.15f * 0.0 - hash_f() * 1.0 / 30;
	float md = 5.0;

	float env = ((floor(t)) % md);
	float env_next = ((floor(t + 1)) % md);
	float env_fr = frac(t);

	env = lerp(
		env,
		env_next,
		smoothstep(0.0, 1.0,
			smoothstep(0.0, 1.0,
				smoothstep(0.0, 1.0,
					smoothstep(0.0, 1.0,
						env_fr
					)
				)
			)
		)
	);

	float3 p = hash_v3()*2.0 - 1.0;
	p *= 0.;

	float focus_dist = (_DOF_FOCAL_DIST*2.0 - 1.0)*5.0;

	//focus_dist = muv.y;

	float dof_fac = 1.0 / float2(R.x/R.y, 1.0) * _DOF_AMOUNT;

	float vert_cnt = 12.0 + env;
	float2 verts[5] = {
		float2(sin(1.0 / vert_cnt * TAU), cos(1.0 / vert_cnt * TAU)),
		float2(sin(2.0 / vert_cnt * TAU), cos(2.0 / vert_cnt * TAU)),
		float2(sin(3.0 / vert_cnt * TAU), cos(3.0 / vert_cnt * TAU)),
		float2(sin(4.0 / vert_cnt * TAU), cos(4.0 / vert_cnt * TAU)),
		float2(sin(5.0 / vert_cnt * TAU), cos(5.0 / vert_cnt * TAU)),
	};

	[unroll(32)]
	for(int i = 0; i < iters; ++i) {
		float r = hash_f();

		float f_idx = floor(r * vert_cnt * (0.99));

		float3 next_p = float3(
			sin(f_idx / vert_cnt * TAU * (1.0 + env * 1.0) + sin(offset.x) * 0.4 * float(floor(offset.x / PI + 1) % 3 > 1)),
			cos(f_idx / vert_cnt * TAU + sin(offset.x) * 0.4 * float(floor(offset.x / PI) % 3 > 1.0)),
			sin(f_idx / vert_cnt * TAU * 4.4 + env + sin(offset.x * 1.0 + 4.0) * 0.4)
		);

		p = sin(p * 1.0 + next_p * 2.0 - env * 0.1);
		if(hash_f() < 0.0 + sin_add(offset.x + 4.0) * 0.4) {
			p += p * 1.0;
			p = mul(p, rot_z(t * 0.1));
			p = mul(p, rot_y(t * 0.1));
		}

		if(hash_f() < 0.0) {
			p += next_p * 1.0;
			p.z -= 0.1;
		}

		float3 q = proj_particle(p);
		float2 k = q.xy;

		k += sample_disk() * abs(q.z - focus_dist + sin(t + sin(t) + f_idx * 0.5)) * 0.05 * dof_fac;

		float2 uv = k.xy / 2.0 + 0.5;
		uint2 cc = uint2(uv.xy * R.xy);
		uint idx = cc.x + Ru.x * cc.y;
		if(
			uv.x > 0.0 && uv.x < 1.0
			&& uv.y > 0.0 && uv.y < 1.0
			&& idx < uint(Ru.x * Ru.y)
		) {
			InterlockedAdd(hist_atomic[idx], 1);
			InterlockedAdd(hist_atomic[idx + Ru.x * Ru.y], uint(r*2400.0));
		}
	}
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint2 res;
	buffer.GetDimensions(res.x, res.y);
	if(dispatchThreadID.x >= res.x || dispatchThreadID.y >= res.y) { return; }
	R = float2(res);
	U = float2(dispatchThreadID.xy);
	float2 U = float2(float(dispatchThreadID.x) + 0.5, float(res.y - dispatchThreadID.y) - 0.5);

	uint hist_id = dispatchThreadID.x + R.x * dispatchThreadID.y;

	float3 col = float(hist_atomic[hist_id]) * float3(3.0,3.0,3.0);
	float3 col_pal = float(hist_atomic[hist_id + res.x * res.y]) * float3(3.0,3.0,3.0);

	k_cols[1] = mul(k_cols[1], rot_y((1.0-0.347)*log(col.x * 1.0) * 4.0 + col_pal.x * 0.0001 * ((1.0 - sin_add(offset.x)) * 0.5)));

	float sc = 124452.7;
	col = log(col * (0.3 + _B)) / log(sc);

	col = pow(col, float3(1.0, 1.0, 1.0)) * mix_cols(col.x / 10);
	
	col = max(col, float3(0.1, 0.1, 0.1));
	col = pow(col, float3(1.0 / 0.25454545f, 1.0 / 0.25454545f, 1.0 / 0.25454545f)) * 20.0;
	col = min(col, float3(0.6, 0.6, 0.6));

	col = clamp(col, float3(0.002, 0.002, 0.002), float3(0.8, 0.8, 0.8));

	buffer[dispatchThreadID.xy] = float4(col, 1.0);
	uint orig_val;
	InterlockedExchange(hist_atomic[hist_id], 0, orig_val);
	InterlockedExchange(hist_atomic[hist_id + res.x * res.y], 0, orig_val);
}