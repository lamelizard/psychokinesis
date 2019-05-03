//based on RTGLive
uniform mat4 uPVM;
uniform int uMode;

in vec3 aPosition;

out float vMode;
out vec4 vNdcPosition;

void main()
{
    vMode = float(uMode);
    vNdcPosition = uPVM * vec4(aPosition, 1);
    gl_Position = vNdcPosition;
}