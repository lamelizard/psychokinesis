in vec3 vNormal;

out vec3 fAlbedo;
out vec3 fNormal;
out vec2 fMaterial;

#if __VERSION__ >= 400
layout(early_fragment_tests) in;
#endif

void main()
{
    fNormal = normalize(vNormal); // bad, should pass flat normalized normal
    fMaterial.xy = vec2(1., .2);
    fAlbedo = vec3(.3,.3,.3);
}
