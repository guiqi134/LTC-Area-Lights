#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out VS_OUT
{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
	mat3 TBN;
} vs_out;

uniform mat4 model;

void main()
{
	mat3 normalMatrix = transpose(inverse(mat3(model)));

	vs_out.fragPos = vec3(model * vec4(aPos, 1.0));
	vs_out.texCoords = aTexCoords;
	vs_out.normal = normalMatrix * aNormal;

	vec3 T = normalize(normalMatrix * aTangent);
	vec3 N = vs_out.normal;
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(N, T);

	mat3 TBN = mat3(T, B, N);
	vs_out.TBN = TBN;

	//gl_Position = projection * view * model * vec4(aPos, 1.0);
}