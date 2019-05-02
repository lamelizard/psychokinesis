uniform mat4 uView;
uniform mat4 uProj;
//uniform mat4 uModel;

in vec3 aPosition;
in vec3 aNormal;
in vec3 aTangent;
in vec2 aTexCoord;
// instanced
in mat4 aModel;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vTangent;
out vec2 vTexCoord;

void main()
{
    // assume uModel has no non-uniform scaling
    vNormal = mat3(aModel) * aNormal;
    vTangent = mat3(aModel) * aTangent;

    vTexCoord = aTexCoord;

    vWorldPos = vec3(aModel * vec4(aPosition, 1));
    gl_Position = uProj * uView * vec4(vWorldPos, 1);
}