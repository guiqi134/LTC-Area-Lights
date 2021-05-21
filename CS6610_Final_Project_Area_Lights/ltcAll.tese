#version 460 core

layout (quads, equal_spacing, ccw) in;

in CS_OUT
{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
	mat3 TBN;
} es_in[];

out ES_OUT
{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
	mat3 TBN;
} es_out;

// handle transforms
uniform mat4 view;
uniform mat4 projection;

// displacement map
uniform sampler2D dispMap;
uniform int planeType;

// ripple effect
uniform bool ripple;
uniform float time;
const float A = 0.2;
const float FREQUENCY = 1.5;

vec2 interpolate2D(vec2 v0, vec2 v1, vec2 v2, vec2 v3)
{
    vec2 a = mix(v0, v1, gl_TessCoord.x);
    vec2 b = mix(v3, v2, gl_TessCoord.x);
    return mix(a, b, gl_TessCoord.y);
}

vec3 interpolate3D(vec3 v0, vec3 v1, vec3 v2, vec3 v3)
{
    vec3 a = mix(v0, v1, gl_TessCoord.x);
    vec3 b = mix(v3, v2, gl_TessCoord.x);
    return mix(a, b, gl_TessCoord.y);
}

void main()
{
	// Interpolate vertex attributes
    es_out.fragPos = interpolate3D(es_in[0].fragPos, es_in[1].fragPos, es_in[2].fragPos, es_in[3].fragPos);
    es_out.normal = interpolate3D(es_in[0].normal, es_in[1].normal, es_in[2].normal, es_in[3].normal);
    es_out.texCoords = interpolate2D(es_in[0].texCoords, es_in[1].texCoords, es_in[2].texCoords, es_in[3].texCoords);
	es_out.TBN = es_in[0].TBN;

	// displace the vertex along the normal
	if (planeType == 0) 
	{
		// ripple effect
		if (ripple)
		{
			vec3 pos = es_out.fragPos;
			float d = sqrt(pos.x * pos.x + pos.z * pos.z);
			es_out.fragPos.y = A * sin(FREQUENCY * d - 5.0 * time);

			// TODO: calculate the derivation of sine function in the direction of this fragment position
			// to reconstruct the normals.
		}
	}
	else
	{
		float height = texture(dispMap, es_out.texCoords).r;
		es_out.fragPos.y += 0.4 * height;
	}

	gl_Position = projection * view * vec4(es_out.fragPos, 1.0);
}