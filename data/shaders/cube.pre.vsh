uniform mat4 uView;
uniform mat4 uProj;
//uniform mat4 uModel;

in vec3 aPosition;
// instanced
in mat4 aModel;

invariant gl_Position;

void main()
{
    gl_Position = uProj * uView * aModel * vec4(aPosition, 1);
}