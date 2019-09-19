uniform sampler2DRect uTexColor;
uniform sampler2DRect uTexNormal;
uniform sampler2DRect uTexMaterial;
uniform sampler2DRect uTexDepth;
uniform sampler2DRect uTexMode;
uniform samplerCube uSkybox;
uniform sampler2D uTexPaper;

uniform mat4 uInvProj;
uniform mat4 uInvView;
uniform mat4 uView;
uniform vec3 uLightPos;
uniform vec3 uCamPos;
uniform float uTime;
uniform float uSkyFactor;

//shadow
uniform sampler2DRectShadow uTexShadow;
uniform float uTexShadowSize;
uniform mat4 uShadowViewProjMatrix;

uniform float uZNear;
uniform float uZFar;



in vec2 vPosition;
in vec3 vDiscoColor;

out vec4 fColor;

// from glow samples (modified)
float zOf(ivec2 uv)
{
    //return -vec4(uView * vec4(worldPos, 1.0)).z;
    float zNormalised = 2.0 * texelFetch(uTexDepth, uv).x - 1.0;
    return 2.0 * uZNear * uZFar / (uZFar + uZNear - zNormalised * (uZFar - uZNear));
}
// from glow samples (modified)
bool isEdge(ivec2 uv)
{
    float OutlineNormal = 2.;
    float depthFactor = 1;

    vec3 n = texelFetch(uTexNormal, uv).xyz;
    vec3 n1 = texelFetch(uTexNormal, uv + ivec2(1,0)).xyz;
    vec3 n2 = texelFetch(uTexNormal, uv + ivec2(-1,0)).xyz;
    vec3 n3 = texelFetch(uTexNormal, uv + ivec2(0,1)).xyz;
    vec3 n4 = texelFetch(uTexNormal, uv + ivec2(0,-1)).xyz;

    float OutlineDepth = n.x * depthFactor; // n.x = dot(n, vec3(0,0,1))

    float d = zOf(uv);
    float d1 = zOf(uv + ivec2(1,0));
    float d2 = zOf(uv + ivec2(-1,0));
    float d3 = zOf(uv + ivec2(0,1));
    float d4 = zOf(uv + ivec2(0,-1));

    float e = 0.0;

    e += distance(n, n1) * OutlineNormal;
    e += distance(n, n2) * OutlineNormal;
    e += distance(n, n3) * OutlineNormal;
    e += distance(n, n4) * OutlineNormal;

    e += abs(d - d1) * OutlineDepth;
    e += abs(d - d2) * OutlineDepth;
    e += abs(d - d3) * OutlineDepth;
    e += abs(d - d4) * OutlineDepth;

    return e > 1;
}
// from glow samples
vec3 shadingSpecularGGX(vec3 N, vec3 V, vec3 L, float roughness, vec3 F0)
{
    // see http://www.filmicworlds.com/2014/04/21/optimizing-ggx-shaders-with-dotlh/
    vec3 H = normalize(V + L);

    float dotLH = max(dot(L, H), 0.0);
    float dotNH = max(dot(N, H), 0.0);
    float dotNL = max(dot(N, L), 0.0);
    float dotNV = max(dot(N, V), 0.0);

    float alpha = roughness * roughness;

    // D (GGX normal distribution)
    float alphaSqr = alpha * alpha;
    float denom = dotNH * dotNH * (alphaSqr - 1.0) + 1.0;
    float D = alphaSqr / (denom * denom);
    // no pi because BRDF -> lighting

    // F (Fresnel term)
    float F_a = 1.0;
    float F_b = pow(1.0 - dotLH, 5); // manually?
    vec3 F = mix(vec3(F_b), vec3(F_a), F0);

    // G (remapped hotness, see Unreal Shading)
    float k = (alpha + 2 * roughness + 1) / 8.0;
    float G = dotNL / (mix(dotNL, 1, k) * mix(dotNV, 1, k));
    // '* dotNV' - canceled by normalization

    // '/ dotLN' - canceled by lambert
    // '/ dotNV' - canceled by G
    return D * F * G / 4.0;
}

//Black -> only borders visible

void main()
{
    
    vec3 color = vec3(0,0,0);
    float depth = texture(uTexDepth, gl_FragCoord.xy).x;
    vec3 worldPos = vec3(0,0,0); // valid if depth < 1

    if (depth < 1) // opaque
    {
        ivec2 uv = ivec2(gl_FragCoord.xy);
        int mode = int(texelFetch(uTexMode, uv).x + .5);

        //I have z-fighting with triangles at an 90° angle, just move one texel
        vec3 N = texelFetch(uTexNormal, uv).xyz;
        vec3 N1 = texelFetch(uTexNormal, uv + ivec2(1,0)).xyz;
        vec3 N2 = texelFetch(uTexNormal, uv + ivec2(-1,0)).xyz;
        vec3 N3 = texelFetch(uTexNormal, uv + ivec2(0,1)).xyz;
        vec3 N4 = texelFetch(uTexNormal, uv + ivec2(0,-1)).xyz;
        if(N != vec3(0.,0.,0.) && abs(dot(N,N1)) + abs(dot(N,N2)) + abs(dot(N,N3)) + abs(dot(N,N4)) < .5){
            uv += ivec2(0,1);
            N = N3;
            //fColor = vec3(1.,0.,0.);
            //return;
        }
        vec3 albedo = texelFetch(uTexColor, uv).rgb;

        //get worldspace Pos
        //https://stackoverflow.com/questions/32227283/getting-world-position-from-depth-buffer-value
        vec4 clipSpacePosition = vec4(vPosition * 2.0 - 1.0, depth * 2. - 1., 1.0);
        vec4 viewSpacePosition = uInvProj * clipSpacePosition;
        viewSpacePosition /= viewSpacePosition.w;
        worldPos = (uInvView * viewSpacePosition).xyz;

        //shadow, glow samples
        vec4 shadowPos = uShadowViewProjMatrix * vec4(worldPos, 1.0);
        shadowPos.xyz /= shadowPos.w;
        vec3 L = normalize(uLightPos - worldPos);
        float bias = -0.005 * tan(acos(dot(N, L)));
#if __VERSION__ >= 400
        float shadowFactor = texture(uTexShadow, vec3((shadowPos.xy * .5 + .5) * uTexShadowSize, shadowPos.z * .5 + .5 + bias)).r;
#else
        float shadowFactor = 1.;
#endif
        shadowFactor *= 1 - length(shadowPos.xy) / sqrt(2);


        //Reflection
        //modified from glow samples
        vec2 material = texelFetch(uTexMaterial, uv).xy;
        float Metallic = material.x;
        float Roughness = material.y;
        vec3 V = normalize(uCamPos - worldPos);
        vec3 R = reflect(-V, N);

        if (mode < .1){
            //modified from glow samples

            vec3 diffuse = albedo * (1 - Metallic);
            vec3 specular = mix(vec3(0.04), albedo, Metallic); // fixed spec for non-metals
            float reflectivity = 0.05 * Metallic;

            float lod = Roughness * 15; // 15?
            vec3 reflection = textureLod(uSkybox, R, lod).rgb;


            float dotNL = dot(N, L);
            float dotRL = dot(R, L);

            vec3 lightColor = vec3((max(dot(N, L), 0.) * shadowFactor * 0.9 + 0.1)); // make more red?

           // color += vec3(0.04); // ambient
            color += lightColor * diffuse * max(0.0, dotNL); // lambert
            color += lightColor * shadingSpecularGGX(N, V, L, max(0.01, Roughness), specular); // ggx
            color += reflectivity * reflection; // reflection

        }
        else if (mode == 1){
           // ~Neon
           float inten[5] = float[5](.0f, .5f, .7f, 1.f, 1.f); // use sin(time)!!!
           color = albedo;
           // 4^3 colors
           color = vec3(inten[int(color.r * 4)], inten[int(color.g * 4)], inten[int(color.b * 4)]);
        }
        else if (mode == 2){
            // drawn
            // idea:
            //http://www.thomaseichhorn.de/npr-sketch-shader-vvvv/

            //noise
            //https://www.shadertoy.com/view/4djSRW
#define ITERATIONS 2
            float noise = 0.0;
            for (int t = 0; t < ITERATIONS; t++)
            {
                 float v = float(t+1)*.152;
                 vec2 pos = (gl_FragCoord.xy * v + 0 /*mod(uTime, 10.f)*/ * 1500. + 50.0);
                 vec3 p3  = fract(vec3(pos.xyx) * .1031);
                 p3 += dot(p3, p3.yzx + 19.19);
                 noise += fract((p3.x + p3.y) * p3.z);
            }
            noise = noise / float(ITERATIONS);
            //noise = clamp(noise *2, .0, 1.);

            vec3 paper = texture(uTexPaper, vPosition).rgb;
            //float grey = dot(albedo, vec3(0.21, 0.71, 0.07));

            color  = vec3(.7,.7,.7);// * (max(dot(n, l), 0.) * shadowFactor * 0.9 + 0.1);
            float dFactor = clamp((zOf(uv) -3) / 10., .1, 1.);

            color = color * ((1-dFactor) * noise + dFactor);
            color *= paper;

            if(isEdge(uv))
                color *= dFactor; // black outline fades away in the distance
            //color  = vec3(.7,.7,.7) * shadowFactor;
            //color = vec3(noise);
        }
        else if (mode == 3){
            // disco
            float refGrey = dot(textureLod(uSkybox, R, 4).rgb, vec3(0.21, 0.71, 0.07));
            color = (albedo * .03 + .97 * refGrey * vDiscoColor) * (1 - shadowFactor) * 3;


        }

    }
    //else // sky, from rtglive
    if(depth == 1 || worldPos.y < - .5)
    {
        vec4 viewNear = uInvProj * vec4(vPosition * 2 - 1, 0, 1);
        vec4 viewFar = uInvProj * vec4(vPosition * 2 - 1, 1, 1);
        viewNear /= viewNear.w;
        viewFar /= viewFar.w;
        vec4 worldNear = uInvView * viewNear;
        vec4 worldFar = uInvView * viewFar;
        vec3 dir = worldFar.xyz - worldNear.xyz;
        vec3 skycolor = texture(uSkybox, dir).rgb * uSkyFactor;
        if(depth == 1)
            color = skycolor;
        else{
            float alpha = clamp((worldPos.y+.5)/-9.5,0.,1.); // linear  // smoothstep(-.5, -15., worldPos.y);
            color = alpha * skycolor + (1 - alpha) * color;
        }

    }

    fColor.xyz = color;
    fColor.w = sqrt(dot(color, vec3(0.299, 0.587, 0.114)));//luma for fxaa

}
