#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in float vHeight01;

out vec4 FragColor;

uniform vec3 uLightPos;
uniform vec3 uLightIntensity;

uniform sampler2D uTexSand;   // low
uniform sampler2D uTexGrass;  // mid
uniform sampler2D uTexRock;   // steep / mid-high
uniform sampler2D uTexSnow;   // high

uniform bool  uUseTextures;

uniform float uAmbient;       // ~0.06-0.12
uniform float uGamma;         // 2.2
uniform float uExposure;      // 1.0-1.4

uniform float uSnowLine;      // 0..1
uniform float uSlopeRock;     // 0..1
uniform int   uThemeMode;     // 0 snow, 1 grass (wonderland)

// Simple tonemap
vec3 tonemap(vec3 x) {
    return vec3(1.0) - exp(-x * uExposure);
}

// Hash function for procedural patterns
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// 2D noise for grass variation
float noise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Enhanced ambient occlusion based on slope and height
float computeAO(float slope, float height01) {
    float heightAO = smoothstep(0.0, 0.25, height01);
    float slopeAO = 1.0 - slope * 0.25;
    return mix(0.7, 1.0, heightAO * slopeAO);
}

// Soft contact shadows approximation
float softShadow(vec3 N, vec3 L, float ndotl) {
    float shadow = 1.0;
    if (ndotl < 0.15) {
        shadow = smoothstep(0.0, 0.15, ndotl);
    }
    return shadow;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(-vWorldPos);

    float ndotl = max(dot(N, L), 0.0);

    // slope: 0 flat, 1 steep
    float slope = 1.0 - clamp(N.y, 0.0, 1.0);

    // Texture masks
    float rockMask = smoothstep(uSlopeRock - 0.1, uSlopeRock + 0.1, slope);

    // Lowlands mask - meadow valleys
    float lowMask = 1.0 - smoothstep(0.15, 0.35, vHeight01);
    
    // World position based variation for wonderland grass
    vec2 worldUV = vWorldPos.xz * 0.01;
    float grassVar = noise2D(worldUV * 3.0);
    float grassVar2 = noise2D(worldUV * 7.0 + 50.0);

    vec3 baseColor;

    // === WONDERLAND GRASS THEME (only theme) ===
    
    // Rich varied grass colors
    vec3 grassDark = vec3(0.15, 0.45, 0.12);    // Deep forest green
    vec3 grassMid = vec3(0.25, 0.58, 0.18);     // Vibrant meadow green
    vec3 grassLight = vec3(0.35, 0.68, 0.25);   // Sunlit grass
    vec3 grassYellow = vec3(0.55, 0.65, 0.20);  // Dry grass patches
    
    // Blend grass variations based on noise
    vec3 grass1 = mix(grassDark, grassMid, grassVar);
    vec3 grass2 = mix(grassMid, grassLight, grassVar2);
    vec3 grassBase = mix(grass1, grass2, sin(vWorldPos.x * 0.02 + vWorldPos.z * 0.015) * 0.5 + 0.5);
    
    // Add subtle yellow patches in sunny areas
    float sunPatch = smoothstep(0.6, 0.8, grassVar * grassVar2 + vHeight01 * 0.3);
    grassBase = mix(grassBase, grassYellow, sunPatch * 0.25);
    
    // Wildflower accent colors (very subtle)
    vec3 flowerYellow = vec3(0.95, 0.85, 0.3);
    vec3 flowerWhite = vec3(0.95, 0.95, 0.90);
    vec3 flowerPurple = vec3(0.6, 0.4, 0.7);
    
    float flowerNoise = noise2D(worldUV * 25.0);
    float flowerMask = smoothstep(0.85, 0.92, flowerNoise) * (1.0 - slope) * (1.0 - rockMask);
    
    vec3 flowerColor = mix(flowerYellow, flowerWhite, step(0.5, hash(floor(worldUV * 25.0))));
    flowerColor = mix(flowerColor, flowerPurple, step(0.7, hash(floor(worldUV * 25.0 + 100.0))));
    
    grassBase = mix(grassBase, flowerColor, flowerMask * 0.6);
    
    // Valley meadow - slightly richer green
    vec3 valleyGrass = vec3(0.18, 0.52, 0.15);
    grassBase = mix(grassBase, valleyGrass, lowMask * 0.4);
    
    // Soft dirt paths in low areas
    vec3 dirtPath = vec3(0.55, 0.45, 0.32);
    float pathNoise = noise2D(worldUV * 4.0);
    float pathMask = smoothstep(0.7, 0.85, pathNoise) * lowMask * 0.3;
    grassBase = mix(grassBase, dirtPath, pathMask);
    
    // Rocky outcrops on steep slopes
    vec3 rockColor = vec3(0.45, 0.42, 0.38);
    vec3 mossyRock = mix(rockColor, vec3(0.35, 0.45, 0.30), 0.3);
    
    baseColor = mix(grassBase, mossyRock, rockMask);

    // Enhanced lighting
    vec3 diffuse = uLightIntensity * ndotl;
    
    // Specular (Blinn-Phong) - subtle for grass
    vec3 H = normalize(L + V);
    float ndoth = max(dot(N, H), 0.0);
    float shininess = 16.0;
    float specStrength = 0.08; // Subtle grass shine
    
    if (rockMask > 0.5) {
        specStrength = 0.12;
        shininess = 12.0;
    }
    
    vec3 specular = uLightIntensity * specStrength * pow(ndoth, shininess);
    
    // Ambient occlusion
    float ao = computeAO(slope, vHeight01);
    
    // Soft shadows
    float shadow = softShadow(N, L, ndotl);
    
    // Combine lighting
    vec3 ambient = vec3(uAmbient) * ao;
    vec3 lighting = ambient + (diffuse + specular) * shadow;
    vec3 color = baseColor * lighting;

    // Atmospheric fog - dreamy wonderland feel
    float dist = length(vWorldPos.xz);
    float fog = clamp(dist / 8000.0, 0.0, 1.0);
    fog = fog * fog; // Quadratic falloff for more natural look
    
    // Wonderland: warm golden-green haze
    vec3 fogColor = vec3(0.65, 0.78, 0.60);
    color = mix(color, fogColor, fog * 0.4);

    // Tonemap + gamma
    color = tonemap(color);
    color = pow(color, vec3(1.0 / uGamma));

    FragColor = vec4(color, 1.0);
}
