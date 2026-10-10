OGRE_NATIVE_GLSL_VERSION_DIRECTIVE
//-----------------------------------------------------------------------------
// Program Type: fragment
// Language: glsl
// Created by Ogre RT Shader Generator. All rights reserved.
//-----------------------------------------------------------------------------
#define USE_OGRE_FROM_FUTURE
#include <OgreUnifiedShader.h>
SAMPLER2D(gTextureSampler0, 0);
layout(location = 0) uniform	vec4	derived_ambient_light_colour;
layout(location = 1) uniform	vec4	surface_emissive_colour;
layout(location = 2) uniform	vec4	derived_scene_colour;
layout(location = 3) uniform	vec4	light_position_view_space_array[1];
layout(location = 4) uniform	vec4	light_direction_view_space_array[1];
layout(location = 5) uniform	vec4	light_attenuation_array[1];
layout(location = 6) uniform	vec4	spotlight_params_array[1];
layout(location = 7) uniform	vec4	derived_light_diffuse_colour_array[1];

//-----------------------------------------------------------------------------
//                         PROGRAM DEPENDENCIES
//-----------------------------------------------------------------------------
#include "RTSLib_Colour.glsl"
#include "SGXLib_PerPixelLighting.glsl"
#include "FFPLib_Texturing.glsl"

//-----------------------------------------------------------------------------
//                         MAIN
//-----------------------------------------------------------------------------
IN(vec3	vsTexcoord_0, 0)
IN(vec3	vsTexcoord_1, 1)
IN(vec2	vsTexcoord_2, 2)
void main(void) {
	vec4	lColor_0;
	vec4	lColor_1;
	vec4	texel_0;

	lColor_0	=	vec4(1.00000,1.00000,1.00000,1.00000);
	lColor_1	=	vec4(0.00000,0.00000,0.00000,0.00000);
	gl_FragColor	=	lColor_0;
	gl_FragColor	=	derived_scene_colour;
	FFP_Lights(vsTexcoord_0, vsTexcoord_1, light_position_view_space_array, light_attenuation_array, light_direction_view_space_array, spotlight_params_array, derived_light_diffuse_colour_array, gl_FragColor.xyz);
	lColor_0	=	gl_FragColor;
	texel_0	=	texture2D(gTextureSampler0, vsTexcoord_2);
	ENABLE_LINEAR_COLOUR(texel_0);
	gl_FragColor	=	texel_0*gl_FragColor;
	gl_FragColor.xyz	=	gl_FragColor.xyz+lColor_1.xyz;
	COLOUR_TRANSFER(gl_FragColor);
}

