#version 450

layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 frag_uv;
layout(binding = 1) uniform sampler2D tex_sampler;

layout(location = 0) out vec4 outColor;

void main() {
	//outColor = vec4(frag_uv, 0.0, 1.0);
	outColor = texture(tex_sampler, frag_uv);
}