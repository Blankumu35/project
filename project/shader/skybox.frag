#version 330 core
in vec3 vWorldPos;
out vec4 FragColor;

uniform float time;

// Hash function for procedural stars
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
    vec3 dir = normalize(vWorldPos);
    
    // Dark space gradient
    float upness = dot(dir, vec3(0.0, 1.0, 0.0));
    vec3 zenithColor = vec3(0.0, 0.0, 0.05);  // Very dark blue
    vec3 horizonColor = vec3(0.05, 0.0, 0.1); // Dark purple
    vec3 nadirColor = vec3(0.0, 0.0, 0.02);   // Almost black
    
    vec3 skyColor;
    if (upness > 0.0) {
        skyColor = mix(horizonColor, zenithColor, upness);
    } else {
        skyColor = mix(horizonColor, nadirColor, -upness);
    }
    
    // Add stars
    vec2 starUV = dir.xz / (abs(dir.y) + 0.001);
    starUV *= 50.0; // Star density
    
    float starHash = hash(floor(starUV));
    float starBrightness = step(0.98, starHash); // Only brightest become stars
    
    // Twinkle effect
    float twinkle = sin(time * 3.0 + starHash * 100.0) * 0.5 + 0.5;
    starBrightness *= 0.5 + 0.5 * twinkle;
    
    // Star color variation
    vec3 starColor = mix(vec3(1.0, 1.0, 1.0), vec3(0.8, 0.9, 1.0), starHash);
    
    vec3 finalColor = skyColor + starColor * starBrightness * step(0.3, abs(dir.y));
    
    // Add nebula-like clouds
    float nebula = sin(dir.x * 5.0 + time * 0.1) * sin(dir.z * 5.0) * 0.5 + 0.5;
    nebula *= sin(dir.y * 3.0 + time * 0.05) * 0.5 + 0.5;
    vec3 nebulaColor = mix(vec3(0.1, 0.0, 0.2), vec3(0.0, 0.1, 0.2), nebula);
    finalColor += nebulaColor * 0.1 * nebula;
    
    FragColor = vec4(finalColor, 1.0);
}
