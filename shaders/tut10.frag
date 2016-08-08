#version 450

#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack: enable

layout (location = 0) in vec3 in_color;
layout (location = 1) in vec2 in_tex;

layout (set = 0, binding = 0) uniform sampler2D texture_image1;
layout (set = 0, binding = 1) uniform sampler2D texture_image2;

/*
 * Here is how you declare push constants.  It should always be a uniform block, with the layout qualified as
 * push_constant, and where each element has an offset declaration.  Note that GLSL already defines offset, array and
 * matrix strides when declaring a uniform block, so the `layout (offset=XX)` is unnecessary here.  Still, this is
 * useful to know, especially with push constants, since this "offset" is the only thing you tell Vulkan when talking
 * about push constants.
 *
 * Note: I figured this syntax out by reading the grammar accepted by glslang (https://github.com/KhronosGroup/glslang).
 * If you see imprecisions, or know of a proper documentation, do let me know.
 */
layout (push_constant) uniform push_constants
{
	layout (offset = 0) float mix_value;
} constants;

layout (location = 0) out vec4 out_color;

void main()
{
	vec4 tex_color1 = texture(texture_image1, in_tex);
	vec4 tex_color2 = texture(texture_image2, in_tex);
	vec4 tex_color = mix(tex_color1, tex_color2, constants.mix_value);
	out_color = vec4(in_color * tex_color.xyz, 1.0);
}
