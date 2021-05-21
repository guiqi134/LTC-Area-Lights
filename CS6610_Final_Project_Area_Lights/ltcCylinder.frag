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
    float radius;
	vec3 lightColor;
    vec3 points[2];	
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
uniform bool analytic; // use analytic line light to approximate cylinder light?
uniform bool endCaps; // use endCaps?
uniform sampler2D LTC1; // for inverse M
uniform sampler2D LTC2; // GGX norm, fresnel, 0(unused), sphere

const float LUT_SIZE  = 64.0; // ltc_texture size 
const float LUT_SCALE = (LUT_SIZE - 1.0)/LUT_SIZE;
const float LUT_BIAS  = 0.5/LUT_SIZE;
const float PI = 3.14159265;


// code from [Frisvad2012]
// link: https://orbit.dtu.dk/files/126824972/onb_frisvad_jgt2012_v2.pdf
void buildOrthonormalBasis(in vec3 n, out vec3 b1, out vec3 b2)
{
    if (n.z < -0.9999999)
    {
        b1 = vec3( 0.0, -1.0, 0.0);
        b2 = vec3(-1.0,  0.0, 0.0);
        return;
    }
    float a = 1.0 / (1.0 + n.z);
    float b = -n.x*n.y*a;
    b1 = vec3(1.0 - n.x*n.x*a, b, -n.x);
    b2 = vec3(b, 1.0 - n.y*n.y*a, -n.y);
}

mat3 Minv;
// TODO: ???
float D(vec3 w)
{
    // Using Minv to get back to origin distribution
    vec3 wo = Minv * w;
    float lo = length(wo);
    // BRDF * cos
    float res = 1.0/PI * max(0.0, wo.z/lo) * abs(determinant(Minv)) / (lo*lo*lo);
    return res;
}

float I_cylinder_numerical(vec3 p1, vec3 p2, float R)
{
    // init orthonormal basis
    float L = length(p2 - p1);
    vec3 wt = normalize(p2 - p1);
    vec3 wt1, wt2;
    buildOrthonormalBasis(wt, wt1, wt2);

    // integral discretization
    float I = 0.0;
    const int nSamplesphi = 20;
    const int nSamplesl   = 100;
    for (int i = 0; i < nSamplesphi; ++i)
    for (int j = 0; j < nSamplesl;   ++j)
    {
        // normal Eq.(1.3)
        float phi = 2.0 * PI * float(i)/float(nSamplesphi);
        vec3 wn = cos(phi)*wt1 + sin(phi)*wt2;

        // position Eq.(1.4)
        float l = L * float(j)/float(nSamplesl - 1);
        vec3 p = p1 + l*wt + R*wn;

        // normalized direction Eq.(1.5)
        vec3 wp = normalize(p);

        // integrate 
        I += D(wp) * max(0.0, dot(-wp, wn)) / dot(p, p);
    }

    // Eq.(1.2)
    I *= 2.0 * PI * R * L / float(nSamplesphi*nSamplesl);
    return I;
}

float Fpo(float d, float l)
{
    return l/(d*(d*d + l*l)) + atan(l/d)/(d*d);
}

float Fwt(float d, float l)
{
    return l*l/(d*(d*d + l*l));
}

float I_diffuse_line(vec3 p1, vec3 p2)
{
    // tangent
    vec3 wt = normalize(p2 - p1);

    // clamping
    if (p1.z <= 0.0 && p2.z <= 0.0) return 0.0;
    if (p1.z < 0.0) p1 = (+p1*p2.z - p2*p1.z) / (+p2.z - p1.z);
    if (p2.z < 0.0) p2 = (-p1*p2.z + p2*p1.z) / (-p2.z + p1.z);

    // parameterization Eq.(1.12, 1.13)
    float l1 = dot(p1, wt);
    float l2 = dot(p2, wt);

    // shading point orthonormal projection on the line Eq.(1.14)
    vec3 po = p1 - l1*wt;

    // distance to line Eq.(1.15)
    float d = length(po);

    // integral Eq.(1.21)
    float I = (Fpo(d, l2) - Fpo(d, l1)) * po.z +
              (Fwt(d, l2) - Fwt(d, l1)) * wt.z;
    return I / PI;
}

float I_ltc_line(vec3 p1, vec3 p2)
{
    // transform to diffuse configuration
    vec3 p1o = Minv * p1;
    vec3 p2o = Minv * p2;
    float I_diffuse = I_diffuse_line(p1o, p2o);

    // width factor
    vec3 ortho = normalize(cross(p1, p2));
    float w =  1.0 / length(inverse(transpose(Minv)) * ortho);

    return w * I_diffuse;
}

// Integrating the end caps
float I_disks_numerical(vec3 p1, vec3 p2, float R)
{
    // init orthonormal basis
    float L = length(p2 - p1);
    vec3 wt = normalize(p2 - p1);
    vec3 wt1, wt2;
    buildOrthonormalBasis(wt, wt1, wt2);

    // integration
    float Idisks = 0.0;
    const int nSamplesphi = 20;
    const int nSamplesr   = 200;
    for (int i = 0; i < nSamplesphi; ++i)
    for (int j = 0; j < nSamplesr;   ++j)
    {
        float phi = 2.0 * PI * float(i)/float(nSamplesphi);
        float r = R * float(j)/float(nSamplesr - 1);
        vec3 p, wp;

        p = p1 + r * (cos(phi)*wt1 + sin(phi)*wt2);
        wp = normalize(p);
        Idisks += r * D(wp) * max(0.0, dot(wp, +wt)) / dot(p, p);

        p = p2 + r * (cos(phi)*wt1 + sin(phi)*wt2);
        wp = normalize(p);
        Idisks += r * D(wp) * max(0.0, dot(wp, -wt)) / dot(p, p);
    }

    Idisks *= 2.0 * PI * R / float(nSamplesr*nSamplesphi);
    return Idisks;
}

float I_ltc_disks(vec3 p1, vec3 p2, float R)
{
    float A = PI * R * R;
    vec3 wt  = normalize(p2 - p1);
    vec3 wp1 = normalize(p1);
    vec3 wp2 = normalize(p2);
    float Idisks = A * (
    D(wp1) * max(0.0, dot(+wt, wp1)) / dot(p1, p1) +
    D(wp2) * max(0.0, dot(-wt, wp2)) / dot(p2, p2));
    return Idisks;
}

vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N*dot(V, N));
    T2 = cross(N, T1);

    mat3 B = transpose(mat3(T1, T2, N));

    vec3 p1 = B * (light.points[0] - P);
    vec3 p2 = B * (light.points[1] - P);

    if (analytic) // analytic integration
    {
        float Iline = light.radius * I_ltc_line(p1, p2);
        float Idisks = endCaps ? I_ltc_disks(p1, p2, light.radius) : 0.0;
        // there are some bugs when roughness is quite small
        return vec3(min(1.0, Iline + Idisks));
    }
    else // numerical integration
    {
        float Icylinder = I_cylinder_numerical(p1, p2, light.radius);
        float Idisks = endCaps ? I_disks_numerical(p1, p2, light.radius) : 0.0;
        return vec3(Icylinder + Idisks);
    }
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

    // Evaluate LTC specular shading
    Minv = mat3(
        vec3(t1.x, 0, t1.y),
        vec3(  0,  1,    0),
        vec3(t1.z, 0, t1.w)
    );
    vec3 specular = LTC_Evaluate(N, V, fs_in.fragPos);
    // GGX BRDF shadowing and Fresnel
    specular *= mSpecular * t2.x + (1.0 - mSpecular) * t2.y;

    // Evaluate LTC diffuse shading
    Minv = mat3(1.0);
    vec3 diffuse = LTC_Evaluate(N, V, fs_in.fragPos);

    result = light.intensity * light.lightColor * (specular + mDiffuse * diffuse);
    result /= 2.0 * PI;

	fragColor = vec4(result, 1.0);
}