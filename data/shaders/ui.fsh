uniform sampler2D uTexHealth;

in vec2 vPosition;

out vec3 fColor;

void main()
{
        
    vec4 color  = texture(uTexHealth, vPosition);
    if(color.a < 0.99)
      discard; 
        
    // conversion to sRGB
    fColor = pow(color.rgb, vec3(1 / 2.224));
   
}
