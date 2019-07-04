uniform mat4 uProjView;
uniform mat4 uModel;

in vec3 position;
in vec3 normal;

out vec3 vNormal;

invariant gl_Position;

void main()
{
    // assume uModel has no non-uniform scaling
    vNormal = mat3(uModel) * normal;
    if(vNormal.z > 0)
        vNormal = - vNormal;
    gl_Position = uProjView * uModel * vec4(position, 1.0f);
}
