uniform mat4 uProjViewModel;

in vec3 position;
in vec3 color;

out vec3 vColor;

void main()
{
    vColor = color;
    gl_Position = uProjViewModel * vec4(position, 1.0f);
}
