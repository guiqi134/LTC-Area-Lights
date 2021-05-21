#version 460 core

out vec4 fragColor;

in VS_OUT
{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
} fs_in;

struct Light
{
    float intensity;
	vec3 lightColor;
    vec3 points[4];
};
uniform Light light;

struct Material
{
	vec3 diffuse;
	vec3 specular;
	float roughness;
};
uniform Material material;

uniform vec3 cameraPos;
uniform bool twoSided; // two Side lighting
uniform sampler2D LTC1; // for inverse M
uniform sampler2D LTC2; // GGX norm, fresnel, 0(unused), sphere

const float LUT_SIZE  = 64.0; // ltc_texture size 
const float LUT_SCALE = (LUT_SIZE - 1.0)/LUT_SIZE;
const float LUT_BIAS  = 0.5/LUT_SIZE;


// Vector form without project to the plane (dot with the normal)
// Use for proxy sphere clipping
vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
    // Using built-in acos() function will result flaws
    // Using fitting result for calculating acos()
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
    float b = 3.4175940 + (4.1616724 + y)*y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - v;

    return cross(v1, v2)*theta_sintheta;
}

float IntegrateEdge(vec3 v1, vec3 v2)
{
    return IntegrateEdgeVec(v1, v2).z;
}


// P is fragPos in world space (LTC distribution)
vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    // TODO: Is this operation helps to set M_inverse into face's normal due to view direction???
    Minv = Minv * transpose(mat3(T1, T2, N)); 

    // polygon (allocate 5 vertices for clipping)
    vec3 L[5];
    // transform polygon from LTC back to origin Do (cosine weighted) 
    L[0] = Minv * (points[0] - P); 
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);
    L[3] = Minv * (points[3] - P);

    // integrate
    float sum = 0.0;

    // use tabulated horizon-clipped sphere
    // check if the shading point is behind the light
    vec3 dir = points[0] - P; // LTC space
    vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0); 

    // cos weighted space
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    vec3 vsum = vec3(0.0);
    
    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);
    
    // form factor of the polygon in direction vsum
    float len = length(vsum);
    // TODO: ???
    float z = vsum.z/len;
    
    // TODO: ???
    if (behind)
        z = -z;
    
    vec2 uv = vec2(z*0.5 + 0.5, len); // range [0, 1]
    uv = uv*LUT_SCALE + LUT_BIAS;
    
    // TODO: ???
    float scale = texture(LTC2, uv).w;
    
    sum = len*scale;
    
    if (!behind && !twoSided)
        sum = 0.0;
    
    // Out irradiance ???
    vec3 Lo_i = vec3(sum, sum, sum);

    return Lo_i;
}

vec3 PowVec3(vec3 v, float p)
{
    return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

const float gamma = 2.2;
vec3 ToLinear(vec3 v) { return PowVec3(v, gamma); }

void main()
{
    // gamma correction
    vec3 mDiffuse = ToLinear(material.diffuse);
    vec3 mSpecular = ToLinear(material.specular);

    vec3 result = vec3(0.0);

	vec3 N = normalize(fs_in.normal);
	vec3 V = normalize(cameraPos - fs_in.fragPos);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);

    // use roughness and sqrt(1-cos_theta) to sample M_texture
    vec2 uv = vec2(material.roughness, sqrt(1.0 - NdotV));
    uv = uv*LUT_SCALE + LUT_BIAS;   

    // get 4 parameters for inverse_M
    vec4 t1 = texture(LTC1, uv); 

    // Get 2 parameters for Fresnel calculation
    vec4 t2 = texture(LTC2, uv);

    mat3 Minv = mat3(
        vec3(t1.x, 0, t1.y),
        vec3(  0,  1,    0),
        vec3(t1.z, 0, t1.w)
    );

    // Evaluate LTC shading
    vec3 diffuse = LTC_Evaluate(N, V, fs_in.fragPos, mat3(1), light.points, twoSided);
    vec3 specular = LTC_Evaluate(N, V, fs_in.fragPos, Minv, light.points, twoSided);

    // GGX BRDF shadowing and Fresnel
    // t2.x: shadowedF90 ??? (F90 normally it should be 1.0)
    // t2.y: Smith function for Geometric Attenuation Term, it is dot(V or L, H).
    specular *= mSpecular * t2.x + (1.0 - mSpecular) * t2.y;

    result = light.intensity * light.lightColor * (specular + mDiffuse * diffuse);

	fragColor = vec4(result, 1.0);
}
