#version 460 core

layout (vertices = 4) out;

in VS_OUT
{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
	mat3 TBN;
} cs_in[];

out CS_OUT
{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
	mat3 TBN;
} cs_out[];

void main()
{
	cs_out[gl_InvocationID].fragPos = cs_in[gl_InvocationID].fragPos;
	cs_out[gl_InvocationID].normal = cs_in[gl_InvocationID].normal;
	cs_out[gl_InvocationID].texCoords = cs_in[gl_InvocationID].texCoords;
	cs_out[gl_InvocationID].TBN = cs_in[gl_InvocationID].TBN;

	float tessLevel = 64.0f;
	gl_TessLevelOuter[0] = tessLevel;
    gl_TessLevelOuter[1] = tessLevel;
    gl_TessLevelOuter[2] = tessLevel;
    gl_TessLevelOuter[3] = tessLevel;
    gl_TessLevelInner[0] = tessLevel;
    gl_TessLevelInner[1] = tessLevel;
}