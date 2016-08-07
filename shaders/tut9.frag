#version 450

#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack: enable

layout (location = 0) in vec3 in_color;
layout (location = 1) in vec2 in_tex;

layout (set = 0, binding = 0) uniform sampler2D texture_image;

layout (location = 0) out vec4 out_color;

void main()
{
	vec4 tex_color = texture(texture_image, in_tex);
	out_color = vec4(in_color * tex_color.xyz, 1.0);
}
