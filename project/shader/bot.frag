#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;
in vec2 fragUV;
in vec3 localPos;

out vec4 finalColor;

uniform vec3 lightPosition;
uniform vec3 lightIntensity;

// Procedural noise function
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++) {
        value += amplitude * noise(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main()
{
    // Procedural texture coordinates based on position
    vec2 texCoord = localPos.xy * 0.05 + localPos.yz * 0.03;
    
    // Generate procedural patterns
    float pattern1 = fbm(texCoord * 10.0);
    float pattern2 = fbm(texCoord * 20.0 + 5.0);
    float stripes = sin(localPos.y * 50.0) * 0.5 + 0.5;
    
    // Base colors with texture variation
    vec3 primaryColor = vec3(0.15, 0.4, 0.8);   // Blue
    vec3 secondaryColor = vec3(0.1, 0.2, 0.4);  // Dark blue
    vec3 accentColor = vec3(0.9, 0.7, 0.2);     // Gold accent
    
    // Mix colors based on procedural patterns
    vec3 baseColor = mix(primaryColor, secondaryColor, pattern1 * 0.5);
    
    // Add panel lines / seams
    float panels = step(0.95, fract(localPos.y * 15.0)) + step(0.95, fract(localPos.x * 15.0));
    baseColor = mix(baseColor, secondaryColor * 0.5, panels * 0.8);
    
    // Add metallic spots/rivets
    float rivets = smoothstep(0.7, 0.75, pattern2);
    baseColor = mix(baseColor, accentColor, rivets * 0.6);
    
    // Lighting
    vec3 lightDir = lightPosition - worldPosition;
    float lightDist = dot(lightDir, lightDir);
    lightDir = normalize(lightDir);
    float ndotl = clamp(dot(lightDir, worldNormal), 0.0, 1.0);
    vec3 lighting = lightIntensity * ndotl / lightDist;
    
    // Rim lighting for stylized look
    vec3 viewDir = normalize(-worldPosition);
    float rimPower = 1.0 - max(0.0, dot(worldNormal, viewDir));
    rimPower = pow(rimPower, 3.0);
    vec3 rimColor = vec3(0.8, 0.9, 1.0);
    
    // Specular highlight
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(worldNormal, halfDir), 0.0), 32.0);
    vec3 specular = vec3(1.0) * spec * 0.5;
    
    // Combine colors
    vec3 color = baseColor * lighting + specular;
    color = mix(color, rimColor, rimPower * 0.3);
    
    // Tone mapping
    color = color / (1.0 + color);
    
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));
    finalColor = vec4(color, 1.0);
}
