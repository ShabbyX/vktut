#version 450

#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack: enable

layout (location = 0) in vec2 in_tex;

layout (set = 0, binding = 0) uniform sampler2D rendered_image;

layout (push_constant) uniform push_constants
{
	float pixel_size;
	float hue_levels;
	float saturation_levels;
	float intensity_levels;
} constants;

layout (location = 0) out vec4 out_color;

/*
 * rgb2hsv and hsv2rgb are shamelessly copied off the internet:
 *
 *    http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
 */
vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = c.g < c.b ? vec4(c.bg, K.wz) : vec4(c.gb, K.xy);;
    vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec2 levelize(vec2 v, vec2 levels)
{
	return clamp(round(v * levels) / levels, 0.0, 1.0);
}

vec3 levelize(vec3 v, vec3 levels)
{
	return clamp(round(v * levels) / levels, 0.0, 1.0);
}

void main()
{
	vec2 window_size = textureSize(rendered_image, 0);
	vec2 pixelate_levels = window_size / constants.pixel_size;

	vec2 bottom_left = levelize(in_tex, pixelate_levels);
	vec2 window_pixel_length = vec2(1.0) / window_size;
	vec2 top_right = bottom_left + constants.pixel_size * window_pixel_length;

	/*
	 * Sample the image at all pixels inside the fake big pixel and average the colors.
	 *
	 * Note that this is a terrible hit on the performance.  What might one want to do instead
	 * is to ask Vulkan to shrink the image (vkCmdBlitImage), then sample it with VK_FILTER_LINEAR.
	 * The problem here is that this averaging is repeated by each pixel inside the fake big pixel
	 * only to produce the same result.  vkCmdBlitImage would more efficiently do it.
	 */
	float count = 0;
	vec4 sum = vec4(0.0);
	for (float s = bottom_left.s; s < top_right.s; s += window_pixel_length.s)
		for (float t = bottom_left.t; t < top_right.t; t += window_pixel_length.t)
		{
			sum += texture(rendered_image, vec2(s, t));
			count += 1;
		}
	sum /= count;
	sum = clamp(sum, 0.0, 1.0);

	/* Levelize each component of the colors in hsv space. */
	vec3 levels = vec3(constants.hue_levels, constants.saturation_levels, constants.intensity_levels);
	vec3 hsv = rgb2hsv(sum.xyz);
	vec3 rgb = hsv2rgb(levelize(hsv, levels));
	out_color = vec4(rgb.xyz, 1.0);
}
