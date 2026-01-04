#version 330 core
in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragUV;

out vec4 finalColor;

uniform vec3 lightPosition;
uniform vec3 lightIntensity;
uniform vec3 viewPos;
uniform sampler2D baseColorTex;

void main() {
    // DEBUG: fallback color to check if fragment shader runs
    vec3 baseColor = vec3(1.0, 0.2, 0.2); // bright red
    // Uncomment below to use texture
    // baseColor = texture(baseColorTex, fragUV).rgb;
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(lightPosition - fragPosition);
    vec3 V = normalize(viewPos - fragPosition);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 ambient = 0.15 * baseColor;
    vec3 diffuse = diff * baseColor * lightIntensity * 0.0000001;
    vec3 specular = vec3(0.4) * spec;

    vec3 color = ambient + diffuse + specular;
    color = color / (1.0 + color);
    color = pow(color, vec3(1.0 / 2.2));
    finalColor = vec4(color, 1.0);
}
