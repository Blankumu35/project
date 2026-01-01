#version 330 core
in vec4 Color;
out vec4 FragColor;

void main() {
    // Simple circle shape logic
    vec2 circCoord = 2.0 * gl_PointCoord - 1.0; 
    // Since we are drawing quads, we can just output color
    // If you want a smooth dot, you need UVs. 
    // For now, let's just output the aura color.
    FragColor = Color;
}