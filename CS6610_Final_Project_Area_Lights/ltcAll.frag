#version 460 core

#define NUM_LIGHTS 4
#define NUM_POINTS 4
#define MAX_SPHERE_LIGHTS 100 

out vec4 fragColor;

in ES_OUT
{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
	mat3 TBN;
} fs_in;

struct Light
{
    int type; // 0: sphere, 1: rectangle, 2: disk, 3: cylinder
    float radius; // only for cylinder light
    float intensity;
	vec3 lightColor;
    vec3 points[NUM_POINTS];
};
uniform Light lights[NUM_LIGHTS];

struct SphereLight
{
    float intensity;
    vec3 lightColor;
    vec3 points[4];
};
uniform SphereLight sphereLights[MAX_SPHERE_LIGHTS];

struct Material
{
	vec3 diffuse;
	vec3 specular;
	float roughness;

    sampler2D texture_diffuse;
    sampler2D texture_normal;
    sampler2D texture_roughness;
    sampler2D texture_AO;
    sampler2D texture_metallic;
};
uniform Material material;

uniform sampler2D LTC1; // for inverse M
uniform sampler2D LTC2; // GGX norm, fresnel, 0(unused), sphere
uniform int planeType; // 0: Default, 1: stone, 2: marble, 3: wood, 4: diamond plate
uniform vec3 cameraPos;
uniform int numSphereLights;
uniform bool dithering;
uniform mat4 normalMapRot;

const float LUT_SIZE  = 64.0; // ltc_texture size 
const float LUT_SCALE = (LUT_SIZE - 1.0)/LUT_SIZE;
const float LUT_BIAS  = 0.5/LUT_SIZE;
const float PI = 3.14159265;


// -----------------------------------------------------
// utility functions
// -----------------------------------------------------
const float gamma = 2.2;
vec3 PowVec3(vec3 v, float p) { return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p)); }
vec3 ToLinear(vec3 v) { return PowVec3(v, gamma); }

vec3 ScreenSpaceDither( vec2 vScreenPos )
{
    vec3 vDither = vec3( dot( vec2( 171.0, 231.0 ), vScreenPos.xy ) );
    vDither.rgb = fract( vDither.rgb / vec3( 103.0, 71.0, 97.0 ) );
    
    return vDither.rgb / 255.0;
}

// polygon LTC utility function
vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
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

// line LTC utility function
float Fpo(float d, float l) { return l/(d*(d*d + l*l)) + atan(l/d)/(d*d); }
float Fwt(float d, float l) { return l*l/(d*(d*d + l*l)); }

float I_diffuse_line(vec3 p1, vec3 p2)
{
    vec3 wt = normalize(p2 - p1);

    if (p1.z <= 0.0 && p2.z <= 0.0) return 0.0;
    if (p1.z < 0.0) p1 = (+p1*p2.z - p2*p1.z) / (+p2.z - p1.z);
    if (p2.z < 0.0) p2 = (-p1*p2.z + p2*p1.z) / (-p2.z + p1.z);

    float l1 = dot(p1, wt);
    float l2 = dot(p2, wt);

    vec3 po = p1 - l1*wt;

    float d = length(po);

    float I = (Fpo(d, l2) - Fpo(d, l1)) * po.z +
              (Fwt(d, l2) - Fwt(d, l1)) * wt.z;
    return I / PI;
}

float I_ltc_line(vec3 p1, vec3 p2, mat3 Minv)
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

// disk LTC utility function
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


// -----------------------------------------------------
// 2D polygon light LTC (rectangle & star)
// -----------------------------------------------------
vec3 LTC_Evaluate_Polygon(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4])
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    Minv = Minv * transpose(mat3(T1, T2, N)); 

    vec3 L[5];
    L[0] = Minv * (points[0] - P); 
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);
    L[3] = Minv * (points[3] - P);

    float sum = 0.0;

    vec3 dir = points[0] - P; 
    vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0); 

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
    float z = vsum.z/len;
    
    if (behind)
        z = -z;
    
    vec2 uv = vec2(z*0.5 + 0.5, len); // range [0, 1]
    uv = uv*LUT_SCALE + LUT_BIAS;
    
    float scale = texture(LTC2, uv).w;
    sum = len*scale;       
    vec3 Lo_i = vec3(sum, sum, sum);

    return Lo_i;
}


// -----------------------------------------------------
// line light LTC (cylinder)
// -----------------------------------------------------
vec3 LTC_Evaluate_Line(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[2], float radius)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N*dot(V, N));
    T2 = cross(N, T1);

    mat3 B = transpose(mat3(T1, T2, N));

    vec3 p1 = B * (points[0] - P);
    vec3 p2 = B * (points[1] - P);

    float Iline = radius * I_ltc_line(p1, p2, Minv);

    return vec3(min(1.0, Iline));
}


// -----------------------------------------------------
// disk light LTC (disk & sphere)
// -----------------------------------------------------
vec3 LTC_Evaluate_Disk(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4])
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

void main()
{
    vec3 result = vec3(0.0);
    vec2 texCoords = fs_in.texCoords;
    vec3 mDiffuse, mSpecular, normal, N;
    float roughness;
    float AO = (planeType == 1 || planeType == 3) ? texture(material.texture_AO, texCoords).r : 1.0;
    float metallic = planeType == 4 ? texture(material.texture_AO, texCoords).r : 0.05;

    // Default diffuse and specular
    if (planeType == 0)
    {
        mDiffuse = ToLinear(material.diffuse);
        mSpecular = ToLinear(material.specular);
        normal = fs_in.normal;
        N = normalize(normal);
        roughness = material.roughness;
    }
    // diffuse, normal, roughness map
    else 
    {
        vec3 basecolor = texture(material.texture_diffuse, texCoords).rgb;
        basecolor *= AO;
        mDiffuse = ToLinear((1 - metallic) * basecolor);
        mSpecular = ToLinear(metallic * basecolor);
        normal = texture(material.texture_normal, texCoords).rgb; // [0, 1]
        normal = normal * 2.0 - 1.0; // [-1, 1]
        N = normalize(fs_in.TBN * normal);
        //normal = fs_in.normal;
        //N = normalize(normal);
        roughness = texture(material.texture_roughness, texCoords).r;
        result += vec3(0.4) * mDiffuse * AO; // ambient 
    }
    vec3 V = normalize(cameraPos - fs_in.fragPos);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);

    // use roughness and sqrt(1-cos_theta) to sample M_texture
    roughness = max(0.1, roughness); // cannot < 0.08
    vec2 uv = vec2(roughness, sqrt(1.0 - NdotV));
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
    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        int type = lights[i].type;
        vec3 lightPoints[4] = lights[i].points;
        vec3 diffuse = vec3(0.0);
        vec3 specular = vec3(0.0);
        if (type == 1)
        {
            diffuse += LTC_Evaluate_Polygon(N, V, fs_in.fragPos, mat3(1), lightPoints);
            specular += LTC_Evaluate_Polygon(N, V, fs_in.fragPos, Minv, lightPoints);
        }
        else if (type == 3)
        {
            vec3 linePoints[2] = vec3[](lightPoints[0], lightPoints[1]);
            diffuse += LTC_Evaluate_Line(N, V, fs_in.fragPos, mat3(1), linePoints, lights[i].radius);
            specular += LTC_Evaluate_Line(N, V, fs_in.fragPos, Minv, linePoints, lights[i].radius);
        }
        else if (type == 0 || type == 2)
        {
            diffuse += LTC_Evaluate_Disk(N, V, fs_in.fragPos, mat3(1), lightPoints);
            specular += LTC_Evaluate_Disk(N, V, fs_in.fragPos, Minv, lightPoints);
        }
        // GGX BRDF shadowing and Fresnel
        specular *= mSpecular * t2.x + (1.0 - mSpecular) * t2.y;

        vec3 color = lights[i].intensity * lights[i].lightColor * (specular + mDiffuse * diffuse);
        result += type == 3 ? color / (2.0 * PI) : color;
    }
    for (int i = 0; i < numSphereLights; i++)
    {
        vec3 diffuse = LTC_Evaluate_Disk(N, V, fs_in.fragPos, mat3(1), sphereLights[i].points);
        vec3 specular = LTC_Evaluate_Disk(N, V, fs_in.fragPos, Minv, sphereLights[i].points);
        // GGX BRDF shadowing and Fresnel
        specular *= mSpecular * t2.x + (1.0 - mSpecular) * t2.y;
        result += sphereLights[i].intensity * sphereLights[i].lightColor * (specular + mDiffuse * diffuse);
    }

    result += dithering ? ScreenSpaceDither(gl_FragCoord.xy) : vec3(0.0);
	fragColor = vec4(result, 1.0);
}