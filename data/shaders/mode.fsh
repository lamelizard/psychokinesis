//based on RTGLive
uniform mat4 uInvProj;
uniform mat4 uInvView;
uniform vec3 uPos;
uniform float uRadius;
uniform sampler2DRect uTexDepth;

flat in int vMode;
in vec4 vNdcPosition;

out int fMode;

//use include
vec3 unproject(vec4 ndc, mat4 invProj, mat4 invView)
{
    vec4 view = invProj * ndc;
    view /= view.w;
    return vec3(invView * view);
}

void main()
{
    //reconstruct depth
    float depth = texture(uTexDepth, gl_FragCoord.xy).r;
    vec4 ndc = vec4(vNdcPosition.xy / vNdcPosition.w, depth * 2 - 1, 1);
    vec3 P = unproject(ndc, uInvProj, uInvView);
    if(distance(uPos, P) > uRadius)
        discard; 
    fMode = vMode;
}
