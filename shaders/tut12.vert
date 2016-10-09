#version 450

#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack: enable

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_color;

layout (set = 0, binding = 0) uniform transformation_matrices
{
	mat4x4 mvp;	/* model, view and projection matrices pre-calculated */
} transform;

layout (push_constant) uniform push_constants
{
	layout (offset = 0) float angle;
} constants;

layout (location = 0) out vec3 out_color;

void main()
{
	mat4 rotation = mat4(cos(constants.angle), -sin(constants.angle), 0.0, 0.0,
			     sin(constants.angle),  cos(constants.angle), 0.0, 0.0,
			     0.0,                   0.0,                  1.0, 0.0,
			     0.0,                   0.0,                  0.0, 1.0);
	gl_Position = transform.mvp * rotation * vec4(in_pos.xyz, 1);
	out_color = in_color;
}
