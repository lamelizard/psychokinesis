uniform sampler2D uTexColor;
uniform vec2 uResolution;
uniform float ufxaaQualitySubpix;
uniform float ufxaaQualityEdgeThreshold;
uniform float ufxaaQualityEdgeThresholdMin;

in vec2 vPosition;

out vec3 fColor;
#if __VERSION__ >= 400
        #include "FXAA.frag"
#endif


void main()
{
    
    vec3 color = vec3(0,0,0);

    //color  = texture(uTexColor, vPosition).rgb;
#if __VERSION__ >= 400
        #include "FXAA.frag"
    color = FxaaPixelShader(
            vPosition, // where am I?
            FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f), // Console only
            uTexColor, // texture
            uTexColor, uTexColor, // Console only
            1./uResolution, // offset, 'cause sampler2DRect is overrated?
            FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f),	FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f), FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f), // Console only
            ufxaaQualitySubpix,
            ufxaaQualityEdgeThreshold,
            ufxaaQualityEdgeThresholdMin,
            0.f, 0.f, 0.f, FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f) // Console only
        ).rgb;
#else
        color  = texture(uTexColor, vPosition).rgb;
#endif

    // conversion to sRGB
    color = pow(color, vec3(1 / 2.224));

    fColor = color;
   
}
