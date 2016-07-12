#version 450

#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack: enable

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_color;

layout (set = 0, binding = 0) uniform transformation_matrices
{
	mat4x4 mvp;	/* model, view and projection matrices pre-calculated */
} transform;

layout (location = 0) out vec3 out_color;

void main()
{
	gl_Position = transform.mvp * vec4(in_pos.xyz, 1);
	out_color = in_color;
}
