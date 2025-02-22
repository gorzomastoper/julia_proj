#define PI 3.14159265f
#define TAU 6.28318530f

RWTexture2D<float4> buffer : register(u0);
RWBuffer<float2> positions : register(u1);
RWBuffer<float> densities  : register(u2);
RWBuffer<float> properties  : register(u3);

cbuffer scene_cbuffer : register(b0)
{
	float4 offset;
	float4 mouse_pos;
	float4x4 mvp_mtx;
	float4 padding[10]; // For 256-byte size alignment
};

cbuffer simulation_properties : register(b1) {
	float particle_count;
	float smoothing_radius;
	float pressure;
	float ignored2;
	float4 color_a;
	float4 color_b;
	float4 color_c;
	float4 padding_[12]; // For 256-byte size alignment
};

float calculate_pressure_from_density(float density, float target_density, float pressure_multiplier) {
	float density_error = density - target_density;
	float pressure = density_error * pressure_multiplier;
	return pressure;
}

float smoothing_kernel(float dst, float radius) {
	if(dst >= radius) return 0;

	float volume = (PI * pow(radius, 4.0f)) / 6.0f;
	return (radius - dst) * (radius - dst) / volume;
}

float calculate_gradient(float2 sample_point, float smoothing_radius) {
	float property = 0.0f;
	float mass = 1.0f;

	for(uint i = 0 ; i < particle_count; ++i) {
		float dst = length(positions[i].xy - sample_point);
		float influence = smoothing_kernel(dst, smoothing_radius);
		float density = densities[i];
		property += (density == 0) ? 0.0f : -properties[i] * mass * influence / density;
	}

	return property;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	float thickness = 2.0f;

	uint2 res;
	buffer.GetDimensions(res.x, res.y);
	float center_x = res.x / 2;
	float center_y = res.y / 2;
	float4 boundaries_size = mul(float4(mouse_pos.x, mouse_pos.y, 0.0, 1.0f), mvp_mtx);
	// (1.0f - pos.x) / 2.0f) * width
	boundaries_size.x = ((boundaries_size.x) / 2.0f) * res.x;
	boundaries_size.y = ((boundaries_size.y) / 2.0f) * res.y;

	float2 rect_min = float2(center_x - boundaries_size.x * 0.5f, center_y - boundaries_size.y * 0.5f);
	float2 rect_max = float2(center_x + boundaries_size.x * 0.5f, center_y + boundaries_size.y * 0.5f);
	if(dispatchThreadID.x >= res.x || dispatchThreadID.y >= res.y) { return; }
	bool is_outline = (	(dispatchThreadID.x < rect_min.x && dispatchThreadID.x > rect_min.x - thickness && dispatchThreadID.y > rect_min.y && dispatchThreadID.y < rect_max.y) ||
						(dispatchThreadID.x > rect_max.x && dispatchThreadID.x < rect_max.x + thickness && dispatchThreadID.y > rect_min.y && dispatchThreadID.y < rect_max.y) ||
						(dispatchThreadID.y < rect_min.y && dispatchThreadID.y > rect_min.y - thickness && dispatchThreadID.x > rect_min.x && dispatchThreadID.x < rect_max.x) ||
						(dispatchThreadID.y > rect_max.y && dispatchThreadID.y < rect_max.y + thickness && dispatchThreadID.x > rect_min.x && dispatchThreadID.x < rect_max.x));

	// A > x and A < x + outline and A > y and A > y + outline

	float scale = 5.0f;
	float aspect = (float)res.x / (float)res.y;
	float x_advance = (2.0 / (float)res.x * scale * aspect);
	float y_advance = (2.0 / (float)res.y * scale);

	float coord_x = scale * aspect - ((float)dispatchThreadID.x * x_advance);
	float coord_y = -scale + ((float)dispatchThreadID.y * y_advance);
	float2 sample_point = float2(coord_x, coord_y);
	float col = calculate_gradient(sample_point, smoothing_radius);

	float4 color = float4(1.0, 1.0, 1.0, 0.0);
	if(col < 0.0) {
		color = lerp(color, float4(0.0f, 0.0f, 1.0f, 1.0f), (col * -1.0f));
	} else if(col > 0.0f) {
		color = lerp(color, float4(1.0f, 0.0f, 0.0f, 1.0f), col);
	}

	// float4 color = color_a;
	// if(col < 0.0) {
	// 	color = lerp(color, color_b, (col * -1.0f));
	// } else if(col > 0.0f) {
	// 	color = lerp(color, color_c, col);
	// }
	
	if(is_outline) {
		color = float4(1.0, 1.0, 0.0, 1.0);
	} else {
		// color = float4(col, col, col, 1.0);
	}
	
	buffer[dispatchThreadID.xy] = color;
	// buffer[dispatchThreadID.xy] = float4(1.0, 1.0, 0.0, 1.0);
}