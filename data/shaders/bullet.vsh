uniform mat4 uProjView;

in vec3 position;
in vec3 color;

out vec3 vColor;

void main()
{
    gl_Position = uProjView * vec4(position, 1.0f);
    vColor = color;
}
