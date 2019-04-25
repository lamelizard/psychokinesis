uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;
uniform mat4 uBones[64];

in vec3 aPosition;
in vec3 aNormal;
in vec3 aTangent;
in vec2 aTexCoord;
in vec4 aBoneIDs;
in vec4 aBoneWeights;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vTangent;
out vec2 vTexCoord;

void main()
{
    vec4 pos = (uBones[int(aBoneIDs.x)] * vec4(aPosition, 1)) * aBoneWeights.x   //
               + (uBones[int(aBoneIDs.y)] * vec4(aPosition, 1)) * aBoneWeights.y //
               + (uBones[int(aBoneIDs.z)] * vec4(aPosition, 1)) * aBoneWeights.z //
               + (uBones[int(aBoneIDs.w)] * vec4(aPosition, 1)) * aBoneWeights.w;


    // assume uModel has no non-uniform scaling
    vNormal = mat3(uModel) * aNormal;
    vTangent = mat3(uModel) * aTangent;

    vTexCoord = aTexCoord;

    //vWorldPos = vec3(uModel * pos);
    vWorldPos = vec3(pos);
    gl_Position = uProj * uView * vec4(vWorldPos, 1);
}
