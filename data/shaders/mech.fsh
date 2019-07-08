uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;
uniform sampler2D uTexMaterial;

uniform bool uBlink;

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vTangent;
in vec2 vTexCoord;

out vec3 fAlbedo;
out vec3 fNormal;
out vec2 fMaterial;

#if __VERSION__ >= 400
layout(early_fragment_tests) in;
#endif

void main()
{
    // local dirs
    vec3 N = normalize(vNormal);
    vec3 T = normalize(vTangent);
    vec3 B = normalize(cross(N, T));

    // unpack normal map
    vec3 normalMap = texture(uTexNormal, vTexCoord).xyz;
    normalMap.xy = normalMap.xy * 2 - 1;

    // apply normal map
    N = normalize(mat3(T, B, N) * normalMap);
    fNormal = N;

    //material
    vec2 material = texture(uTexMaterial, vTexCoord).ra;
    fMaterial = vec2(material.x, 1 - material.y);

    // read color texture
    if(!uBlink)
        fAlbedo = texture(uTexAlbedo, vTexCoord).rgb;
    else
        fAlbedo = vec3(1.,1.,1.);


}
