uniform sampler2DRect uTexColor;
uniform sampler2DRect uTexNormal;
uniform sampler2DRect uTexDepth;
uniform sampler2DRect uTexMode;
uniform samplerCube uSkybox;

uniform mat4 uInvProj;
uniform mat4 uInvView;
uniform mat4 uView;

uniform float uZNear;
uniform float uZFar;

//rem me
uniform vec3 uLightDir;

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
    float OutlineNormal = 0.6;
    float OutlineDepth = 0.8;

    vec3 n = texelFetch(uTexNormal, uv).xyz;
    vec3 n1 = texelFetch(uTexNormal, uv + ivec2(1,0)).xyz;
    vec3 n2 = texelFetch(uTexNormal, uv + ivec2(-1,0)).xyz;
    vec3 n3 = texelFetch(uTexNormal, uv + ivec2(0,1)).xyz;
    vec3 n4 = texelFetch(uTexNormal, uv + ivec2(0,-1)).xyz;

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
        int mode = int(texelFetch(uTexMode, ivec2(gl_FragCoord.xy)).x + .5);
        if (mode < .1){
        vec3 N = texture(uTexNormal, gl_FragCoord.xy).rgb;
        color  = texture(uTexColor, gl_FragCoord.xy).rgb * max(0.1, dot(normalize(uLightDir), N));
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
           color  = texture(uTexColor, gl_FragCoord.xy).rgb * max(0.1, dot(normalize(uLightDir), N));
           float grey = dot(color, vec3(0.21, 0.71, 0.07));
           int fourGrey = int(grey * 4);
           color = GB[fourGrey];
        }
        else if (mode == 3){
            vec3 N = texture(uTexNormal, gl_FragCoord.xy).rgb;
            color  = texture(uTexColor, gl_FragCoord.xy).rgb * max(0.1, dot(normalize(uLightDir), N));


            //mix(color, vec3(1,0,0), float(isEdge(ivec2(gl_FragCoord.xy))));
            if(isEdge(ivec2(gl_FragCoord.xy)))
                color = vec3(1,0,0);
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
