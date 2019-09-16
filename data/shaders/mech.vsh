uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;
uniform mat4 uBones[64];

in vec3 aPosition;
in vec3 aNormal;
in vec3 aTangent;
in vec2 aTexCoord;
in ivec4 aBoneIDs;
in vec4 aBoneWeights;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vTangent;
out vec2 vTexCoord;

void main()
{

    vec4 iPosition = vec4(aPosition, 1);
    //unity fix
    iPosition.z = -aPosition.y;
    iPosition.y = aPosition.z;
    

    vec4 pos = (uBones[aBoneIDs.x] * iPosition) * aBoneWeights.x   //
               + (uBones[aBoneIDs.y] * iPosition) * aBoneWeights.y //
               + (uBones[aBoneIDs.z] * iPosition) * aBoneWeights.z //
               + (uBones[aBoneIDs.w] * iPosition) * aBoneWeights.w;

    // assume uModel has no non-uniform scaling
    vNormal = mat3(uModel) * aNormal;
    vTangent = mat3(uModel) * aTangent;

    vTexCoord = aTexCoord;

    vWorldPos = vec3(uModel * pos);
    //vWorldPos = vec3(pos);
    gl_Position = uProj * uView * vec4(vWorldPos, 1);

    //if(aBoneWeights.x + aBoneWeights.y + aBoneWeights.z + aBoneWeights.w < 0.999)
      //  gl_Position = vec4(0,0,0,1);
}
