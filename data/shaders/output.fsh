uniform sampler2DRect uTexColor;

in vec2 vPosition;

out vec3 fColor;

//gimme that fxaa

void main()
{
    
    vec3 color = vec3(0,0,0);

    color  = texture(uTexColor, gl_FragCoord.xy).rgb;
        
    // conversion to sRGB
    color = pow(color, vec3(1 / 2.224));

    fColor = color;
   
}
