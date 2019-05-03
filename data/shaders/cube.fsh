uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;



in vec3 vNormal;
in vec3 vTangent;
in vec2 vTexCoord;

out vec3 fAlbedo;
out vec3 fNormal;
//out vec3 fMaterial;

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
    fNormal = normalize(mat3(T, B, N) * normalMap);
    
    // read color texture
    fAlbedo = texture(uTexAlbedo, vTexCoord).rgb;
}
