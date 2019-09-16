in vec2 aPosition;

out vec2 vPosition;
out vec3 vDiscoColor;

uniform float uTime;

void main()
{
    float h = mod(uTime / 4, 1);
    vDiscoColor = clamp(vec3(abs(h * 6 - 3) - 1,
                             2 - abs(h * 6 - 2),
                             2 - abs(h * 6 - 4)),
                             0, 1);

    vPosition = aPosition;
    gl_Position = vec4(aPosition * 2 - 1, 0, 1);
}
