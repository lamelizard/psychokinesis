uniform sampler2DRect uTexColor;
uniform sampler2DRect uTexNormal;
//uniform sampler2DRect uTexDepth;
uniform sampler2DRect uTexMode;
uniform vec3 uLightDir;

out vec3 fColor;

void main()
{
    
    vec3 color = vec3(0,0,0); 
    //float depth = texture(uTexDepth, gl_FragCoord.xy).x;

    int mode = int(texture(uTexMode, gl_FragCoord.xy).x);

    if (mode == 0){
    vec3 N = texture(uTexNormal, gl_FragCoord.xy).rgb;
    color  = texture(uTexColor, gl_FragCoord.xy).rgb * max(0.1, dot(normalize(uLightDir), N));
    }
    else if (mode == 1){
    //Gameboy
    vec3 GB[5] = {  vec3(0.06, 0.22, 0.06),
                    vec3(0.19, 0.39, 0.19),
                    vec3(0.55, 0.67, 0.06),
                    vec3(0.61, 0.74, 0.06),
                    vec3(0.61, 0.74, 0.06)//color = 1 -> int = 4?
                   };
       //vec2 coord = floor(gl_FragCoord.xy * 100) / 99;
       //vec3 N = texture(uTexNormal, coord).rgb;
       //color  = texture(uTexColor, coord).rgb * max(0.1, dot(normalize(uLightDir), N));
       vec3 N = texture(uTexNormal, gl_FragCoord.xy).rgb;
       color  = texture(uTexColor, gl_FragCoord.xy).rgb * max(0.1, dot(normalize(uLightDir), N));
       float grey = dot(color, vec3(0.21, 0.71, 0.07));
       int fourGrey = int(grey * 4);
       color = GB[fourGrey];
    }
        
    // conversion to sRGB
    color = pow(color, vec3(1 / 2.224));

    fColor = color;
   
}
