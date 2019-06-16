uniform sampler2DRect uTexColor;
uniform sampler2DRect uTexNormal;
uniform sampler2DRect uTexDepth;
uniform sampler2DRect uTexMode;
uniform samplerCube uSkybox;

uniform mat4 uInvProj;
uniform mat4 uInvView;
uniform mat4 uView;

//shadow
uniform vec3 uLightPos;
uniform sampler2DRectShadow uTexShadow;
uniform float uTexShadowSize;
uniform mat4 uShadowViewProjMatrix;

uniform float uZNear;
uniform float uZFar;



in vec2 vPosition;

out vec3 fColor;

// from glow samples (modified)

float zOf(ivec2 uv)
{
    //return -vec4(uView * vec4(worldPos, 1.0)).z;
    float zNormalised = 2.0 * texelFetch(uTexDepth, uv).x - 1.0;
    return 2.0 * uZNear * uZFar / (uZFar + uZNear - zNormalised * (uZFar - uZNear));
}

bool isEdge(ivec2 uv)
{
    float OutlineNormal = 1;
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


void main()
{
    
    vec3 color = vec3(0,0,0);
    float depth = texture(uTexDepth, gl_FragCoord.xy).x;

    if (depth < 1) // opaque
    {
        ivec2 uv = ivec2(gl_FragCoord.xy);
        int mode = int(texelFetch(uTexMode, uv).x + .5);
        if (mode < .1){
            //I have z-fighting with triangles at an 90° angle, just move one texel
            vec3 n = texelFetch(uTexNormal, uv).xyz;
            vec3 n1 = texelFetch(uTexNormal, uv + ivec2(1,0)).xyz;
            vec3 n2 = texelFetch(uTexNormal, uv + ivec2(-1,0)).xyz;
            vec3 n3 = texelFetch(uTexNormal, uv + ivec2(0,1)).xyz;
            vec3 n4 = texelFetch(uTexNormal, uv + ivec2(0,-1)).xyz;
            if(abs(dot(n,n1)) + abs(dot(n,n2)) + abs(dot(n,n3)) + abs(dot(n,n4)) < .5){
                uv += ivec2(0,1);
                n = n3;
            }
            //color  = texture(uTexColor, gl_FragCoord.xy).rgb * max(0.1, dot(normalize(uLightDir), n));
            color  = texelFetch(uTexColor, uv).rgb;

            //get worldspace Pos
            //https://stackoverflow.com/questions/32227283/getting-world-position-from-depth-buffer-value
            vec4 clipSpacePosition = vec4(vPosition * 2.0 - 1.0, depth * 2. - 1., 1.0);
            vec4 viewSpacePosition = uInvProj * clipSpacePosition;
            viewSpacePosition /= viewSpacePosition.w;
            vec3 worldPos = (uInvView * viewSpacePosition).xyz;

            //shadow, glow samples
            vec4 shadowPos = uShadowViewProjMatrix * vec4(worldPos, 1.0);
            shadowPos.xyz /= shadowPos.w;
            vec3 l = normalize(uLightPos - worldPos);
            float bias = -0.005 * tan(acos(dot(n, l)));
            float shadowFactor = texture(uTexShadow, vec3((shadowPos.xy * .5 + .5) * uTexShadowSize, shadowPos.z * .5 + .5 + bias) ).r;


            //float refDepth = shadowPos.z * .5 + .5;
            //float shadowFactor = float(refDepth <= shadowDepth);
            //if (shadowPos.w < 0.0f) // fix "behind-the-light"
              //  shadowFactor = 1;

            color = (max(dot(n, l), 0.) * shadowFactor * 0.9 + 0.1) * color;
            // Yes, hardware PCF works:
            //if(shadowFactor != .0 && shadowFactor != 1.)
                //color = vec3(1,0,0);

            //color.r = shadowFactor;

        }
        else if (mode == 1){
           //Gameboy
           vec3 GB[5] = vec3[5](   
                            vec3(0.06, 0.22, 0.06),
                            vec3(0.19, 0.39, 0.19),
                            vec3(0.55, 0.67, 0.06),
                            vec3(0.61, 0.74, 0.06),
                            vec3(0.61, 0.74, 0.06)//color = 1 -> int = 4?
                           );
           //vec2 coord = floor(gl_FragCoord.xy * 100) / 99;
           //vec3 N = texture(uTexNormal, coord).rgb;
           //color  = texture(uTexColor, coord).rgb * max(0.1, dot(normalize(uLightDir), N));
           vec3 N = texture(uTexNormal, gl_FragCoord.xy).rgb;
           color  = texture(uTexColor, gl_FragCoord.xy).rgb /** max(0.1, dot(normalize(uLightDir), N))*/;
           float grey = dot(color, vec3(0.21, 0.71, 0.07));
           int fourGrey = int(grey * 4);
           color = GB[fourGrey];
        }
        else if (mode == 3){
            //http://www.thomaseichhorn.de/npr-sketch-shader-vvvv/
            vec3 N = texture(uTexNormal, gl_FragCoord.xy).rgb;
            vec3 albedo = texelFetch(uTexColor, uv).rgb;
            //float grey = dot(albedo, vec3(0.21, 0.71, 0.07));
            color  = vec3(.5,.5,.5)/* * max(0.1, dot(normalize(uLightDir), N))*/;

            if(isEdge(uv))
                color *= clamp(zOf(uv) / 50., .1, 1.); // black outline fades away in the distance


        }
    }
    else // sky, from rtglive
    {
        vec4 viewNear = uInvProj * vec4(vPosition * 2 - 1, 0, 1);
        vec4 viewFar = uInvProj * vec4(vPosition * 2 - 1, 1, 1);
        viewNear /= viewNear.w;
        viewFar /= viewFar.w;
        vec4 worldNear = uInvView * viewNear;
        vec4 worldFar = uInvView * viewFar;
        vec3 dir = worldFar.xyz - worldNear.xyz;

        color = texture(uSkybox, dir).rgb * 0.1;
    }

    fColor = color;
   
}
