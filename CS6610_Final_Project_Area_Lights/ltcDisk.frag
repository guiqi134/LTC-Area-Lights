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
const float PI = 3.14159265;


// An extended version of the implementation from
// "How to solve a cubic equation, revisited"
// http://momentsingraphics.de/?p=105
vec3 SolveCubic(vec4 Coefficient)
{
    // Normalize the polynomial
    Coefficient.xyz /= Coefficient.w;
    // Divide middle coefficients by three
    Coefficient.yz /= 3.0;

    float A = Coefficient.w;
    float B = Coefficient.z;
    float C = Coefficient.y;
    float D = Coefficient.x;

    // Compute the Hessian and the discriminant
    vec3 Delta = vec3(
        -Coefficient.z*Coefficient.z + Coefficient.y,
        -Coefficient.y*Coefficient.z + Coefficient.x,
        dot(vec2(Coefficient.z, -Coefficient.y), Coefficient.xy)
    );

    float Discriminant = dot(vec2(4.0*Delta.x, -Delta.y), Delta.zy);

    vec3 RootsA, RootsD;

    vec2 xlc, xsc;

    // Algorithm A
    {
        float A_a = 1.0;
        float C_a = Delta.x;
        float D_a = -2.0*B*Delta.x + Delta.y;

        // Take the cubic root of a normalized complex number
        float Theta = atan(sqrt(Discriminant), -D_a)/3.0;

        float x_1a = 2.0*sqrt(-C_a)*cos(Theta);
        float x_3a = 2.0*sqrt(-C_a)*cos(Theta + (2.0/3.0)*PI);

        float xl;
        if ((x_1a + x_3a) > 2.0*B)
            xl = x_1a;
        else
            xl = x_3a;

        xlc = vec2(xl - B, A);
    }

    // Algorithm D
    {
        float A_d = D;
        float C_d = Delta.z;
        float D_d = -D*Delta.y + 2.0*C*Delta.z;

        // Take the cubic root of a normalized complex number
        float Theta = atan(D*sqrt(Discriminant), -D_d)/3.0;

        float x_1d = 2.0*sqrt(-C_d)*cos(Theta);
        float x_3d = 2.0*sqrt(-C_d)*cos(Theta + (2.0/3.0)*PI);

        float xs;
        if (x_1d + x_3d < 2.0*C)
            xs = x_1d;
        else
            xs = x_3d;

        xsc = vec2(-D, xs + C);
    }

    float E =  xlc.y*xsc.y;
    float F = -xlc.x*xsc.y - xlc.y*xsc.x;
    float G =  xlc.x*xsc.x;

    vec2 xmc = vec2(C*F - B*G, -B*F + C*E);

    vec3 Root = vec3(xsc.x/xsc.y, xmc.x/xmc.y, xlc.x/xlc.y);

    if (Root.x < Root.y && Root.x < Root.z)
        Root.xyz = Root.yxz;
    else if (Root.z < Root.x && Root.z < Root.y)
        Root.xyz = Root.xzy;

    return Root;
}

// P is fragPos in world space (LTC distribution)
vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N*dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    mat3 R = transpose(mat3(T1, T2, N));

    // 3 of the 4 vertices around disk
    vec3 L_[3];
    L_[0] = R * (points[0] - P);
    L_[1] = R * (points[1] - P);
    L_[2] = R * (points[2] - P);

    // init ellipse
    vec3 C  = 0.5 * (L_[0] + L_[2]); // center
    vec3 V1 = 0.5 * (L_[1] - L_[2]); // axis 1
    vec3 V2 = 0.5 * (L_[1] - L_[0]); // axis 2

    // back to cosine distribution, but V1 and V2 no longer ortho.
    C  = Minv * C;
    V1 = Minv * V1;
    V2 = Minv * V2;

    // not two sided lighting AND shading point behind light
    if (!twoSided && dot(cross(V1, V2), C) >= 0.0)
        return vec3(0.0);

    // compute eigenvectors of ellipse
    float a, b;
    float d11 = dot(V1, V1); // q11
    float d22 = dot(V2, V2); // q22
    float d12 = dot(V1, V2); // q12
    if (abs(d12)/sqrt(d11*d22) > 0.0001)
    {
        float tr = d11 + d22;
        float det = -d12*d12 + d11*d22;

        // use sqrt matrix to solve for eigenvalues
        det = sqrt(det);
        float u = 0.5*sqrt(tr - 2.0*det);
        float v = 0.5*sqrt(tr + 2.0*det);
        float e_max = (u + v) * (u + v); // e2
        float e_min = (u - v) * (u - v); // e1

        // two eigenvectors
        vec3 V1_, V2_;

        // q11 > q22
        if (d11 > d22)
        {
            V1_ = d12*V1 + (e_max - d11)*V2; // E2
            V2_ = d12*V1 + (e_min - d11)*V2; // E1
        }
        else
        {
            V1_ = d12*V2 + (e_max - d22)*V1;
            V2_ = d12*V2 + (e_min - d22)*V1;
        }

        a = 1.0 / e_max;
        b = 1.0 / e_min;
        V1 = normalize(V1_); // Vx
        V2 = normalize(V2_); // Vy
    }
    else
    {
        // Eigenvalues are diagnoals
        a = 1.0 / dot(V1, V1);
        b = 1.0 / dot(V2, V2);
        V1 *= sqrt(a);
        V2 *= sqrt(b);
    }

    vec3 V3 = cross(V1, V2);
    if (dot(C, V3) < 0.0)
        V3 *= -1.0;

    float L  = dot(V3, C);
    float x0 = dot(V1, C) / L;
    float y0 = dot(V2, C) / L;

    a *= L*L;
    b *= L*L;

    // parameters for solving cubic function
    float c0 = a*b;
    float c1 = a*b*(1.0 + x0*x0 + y0*y0) - a - b;
    float c2 = 1.0 - a*(1.0 + x0*x0) - b*(1.0 + y0*y0);
    float c3 = 1.0;

    // 3D eigen-decomposition: need to solve a cubic function
    vec3 roots = SolveCubic(vec4(c0, c1, c2, c3));

    float e1 = roots.x;
    float e2 = roots.y;
    float e3 = roots.z;

    // direction to front-facing ellipse center
    vec3 avgDir = vec3(a*x0/(a - e2), b*y0/(b - e2), 1.0); // third eigenvector: V-

    mat3 rotate = mat3(V1, V2, V3);

    // transform to V1, V2, V3 basis
    avgDir = rotate*avgDir;
    avgDir = normalize(avgDir);

    // extends of front-facing ellipse
    float L1 = sqrt(-e2/e3);
    float L2 = sqrt(-e2/e1);

    // projected solid angle E, like the length(F) in rectangle light
    float formFactor = L1*L2*inversesqrt((1.0 + L1*L1)*(1.0 + L2*L2));

    // use tabulated horizon-clipped sphere
    vec2 uv = vec2(avgDir.z*0.5 + 0.5, formFactor);
    uv = uv*LUT_SCALE + LUT_BIAS;
    float scale = texture(LTC2, uv).w;

    float spec = formFactor*scale;
    vec3 Lo_i = vec3(spec, spec, spec);

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
    specular *= mSpecular * t2.x + (1.0 - mSpecular) * t2.y;
    
    result = light.intensity * light.lightColor * (specular + mDiffuse * diffuse);

	fragColor = vec4(result, 1.0);
}
