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
uniform int   uThemeMode;     // 0 snow, 1 grass, 2 crater

// Simple tonemap
vec3 tonemap(vec3 x) {
    return vec3(1.0) - exp(-x * uExposure);
}

// Enhanced ambient occlusion based on slope and height
float computeAO(float slope, float height01) {
    // Darker in valleys and steep areas
    float heightAO = smoothstep(0.0, 0.3, height01);
    float slopeAO = 1.0 - slope * 0.3;
    return mix(0.6, 1.0, heightAO * slopeAO);
}

// Soft contact shadows approximation
float softShadow(vec3 N, vec3 L, float ndotl) {
    // Fake soft shadows in crevices
    float shadow = 1.0;
    if (ndotl < 0.1) {
        shadow = smoothstep(0.0, 0.1, ndotl);
    }
    return shadow;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(-vWorldPos); // View direction (assuming camera at origin for simplicity)

    float ndotl = max(dot(N, L), 0.0);

    // slope: 0 flat, 1 steep
    float slope = 1.0 - clamp(N.y, 0.0, 1.0);

    // Texture masks
    float rockMask = smoothstep(uSlopeRock - 0.08, uSlopeRock + 0.08, slope);

    float snowMask = 0.0;
    if (uThemeMode == 0) {
        // snow world: snow much earlier
        snowMask = smoothstep(uSnowLine - 0.10, uSnowLine + 0.06, vHeight01);
    } else {
        // other worlds: almost no snow
        snowMask = smoothstep(0.90, 1.00, vHeight01);
    }

    // Lowlands mask (sand / dirt). Crater world shows more low dirt.
    float lowMask = 1.0 - smoothstep(0.18, 0.38, vHeight01);
    if (uThemeMode == 2) lowMask = 1.0 - smoothstep(0.25, 0.55, vHeight01);

    vec3 baseColor;

    if (uUseTextures) {
        vec3 sand  = texture(uTexSand,  vUV).rgb;
        vec3 grass = texture(uTexGrass, vUV).rgb;
        vec3 rock  = texture(uTexRock,  vUV).rgb;
        vec3 snow  = texture(uTexSnow,  vUV).rgb;

        // Start with grass/sand blend by height
        vec3 lowMid = mix(grass, sand, lowMask);

        // Add rock on slopes
        vec3 withRock = mix(lowMid, rock, rockMask);

        // Add snow on high areas
        baseColor = mix(withRock, snow, snowMask);

        // Crater world: tint slightly warmer/ashy
        if (uThemeMode == 2) {
            baseColor *= vec3(1.08, 0.92, 0.85);
        }
    } else {
        // Procedural fallback (NOT black/white)
        vec3 sand  = vec3(0.75, 0.70, 0.55);
        vec3 grass = vec3(0.20, 0.55, 0.22);
        vec3 rock  = vec3(0.35, 0.35, 0.38);
        vec3 snow  = vec3(0.90, 0.95, 1.00);

        vec3 lowMid = mix(grass, sand, lowMask);
        vec3 withRock = mix(lowMid, rock, rockMask);
        baseColor = mix(withRock, snow, snowMask);

        if (uThemeMode == 2) baseColor *= vec3(1.15, 0.90, 0.80);
    }

    // Enhanced lighting with specular highlights
    // Diffuse component
    vec3 diffuse = uLightIntensity * ndotl;
    
    // Specular component (Blinn-Phong)
    vec3 H = normalize(L + V);
    float ndoth = max(dot(N, H), 0.0);
    float shininess = 32.0;
    
    // Adjust specular based on material
    float specStrength = 0.0;
    if (snowMask > 0.5) {
        specStrength = 0.4; // Snow is reflective
        shininess = 64.0;
    } else if (rockMask > 0.5) {
        specStrength = 0.15; // Rock has some shine
        shininess = 16.0;
    } else if (lowMask > 0.5) {
        specStrength = 0.05; // Sand/dirt is matte
        shininess = 8.0;
    } else {
        specStrength = 0.1; // Grass slight shine from moisture
        shininess = 16.0;
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

    // Slight atmospheric fog to sell scale (optional)
    float dist = length(vWorldPos.xz);
    float fog = clamp(dist / 9000.0, 0.0, 1.0);
    vec3 fogColor = (uThemeMode == 0) ? vec3(0.75, 0.85, 0.95) :
                    (uThemeMode == 1) ? vec3(0.60, 0.85, 0.70) :
                                       vec3(0.75, 0.55, 0.45);
    color = mix(color, fogColor, fog * 0.35);

    // Tonemap + gamma
    color = tonemap(color);
    color = pow(color, vec3(1.0 / uGamma));

    FragColor = vec4(color, 1.0);
}
