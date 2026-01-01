#version 330 core

in vec3 worldPosition;
in vec3 worldNormal; 

out vec3 finalColor;

uniform vec3 lightPosition;
uniform vec3 lightIntensity;

void main()
{
	// Base colors for stylized bot
	vec3 baseColor = vec3(0.2, 0.5, 0.9);  // Blue bot
	vec3 rimColor = vec3(0.8, 0.9, 1.0);   // Bright rim light
	
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
	
	// Combine colors
	vec3 color = mix(baseColor * lighting, rimColor, rimPower * 0.3);

	// Tone mapping
	color = color / (1.0 + color);

	// Gamma correction
	finalColor = pow(color, vec3(1.0 / 2.2));
}
