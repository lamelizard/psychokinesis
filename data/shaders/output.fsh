uniform sampler2DRect uTexColor;
uniform sampler2DRect uTexDepth;
uniform bool uShowPostProcess;

out vec3 fColor;

void main()
{
    vec3 color = texture(uTexColor, gl_FragCoord.xy).rgb;
    float depth = texture(uTexDepth, gl_FragCoord.xy).x;

    // conversion to sRGB
    color = pow(color, vec3(1 / 2.224));

    if (uShowPostProcess && depth < 1)
        color = 1 - color; // invert color for foreground if true

    fColor = color;
}
