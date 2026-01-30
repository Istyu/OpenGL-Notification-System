#version 460 core
in vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_ScreenTexture;
uniform vec3 u_GlowColor;
uniform float u_BlurRadius = 4.0; 
uniform vec2 u_Resolution;

void main()
{
    float alpha = 0.0;
    float totalWeight = 0.0;
    vec2 texelSize = 1.0 / u_Resolution;

    // 9x9 sampling
    for(float x = -4.0; x <= 4.0; x += 1.0) {
        for(float y = -4.0; y <= 4.0; y += 1.0) {
            vec2 offset = vec2(x, y) * texelSize * (u_BlurRadius * 0.5);
            float sampleAlpha = texture(u_ScreenTexture, v_UV + offset).r;
            
            // Gaussian weighting
            float dist = length(vec2(x, y));
            float weight = exp(-(dist * dist) / (2.0 * 2.0 * 2.0)); // sigma = 2.0
            
            alpha += sampleAlpha * weight;
            totalWeight += weight;
        }
    }
    
    alpha /= totalWeight; // Accurate averaging

    // --- THRESHOLD ---
    // If the value is very small (noise), cut it to zero.
    if (alpha < 0.01) discard; 

    // Finer amplification for CoD style
    alpha = pow(alpha * 2.5, 1.2); 
    alpha = clamp(alpha, 0.0, 1.0);

    FragColor = vec4(u_GlowColor, alpha);
}