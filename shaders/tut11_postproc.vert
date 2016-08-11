#version 450

#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack: enable

layout (location = 0) in vec3 in_pos;
layout (location = 2) in vec2 in_tex;

layout (location = 0) out vec2 out_tex;

void main()
{
	gl_Position = vec4(in_pos.xyz, 1);
	out_tex = in_tex;
}
