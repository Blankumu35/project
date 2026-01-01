#version 330 core
out vec4 FragColor;

void main() {
    // 1. Create a circular shape for the point
    // gl_PointCoord gives (0,0) to (1,1) for the square of the point
    vec2 circCoord = 2.0 * gl_PointCoord - 1.0;
    float dist = dot(circCoord, circCoord);

    // 2. Discard pixels outside the circle to avoid square particles
    if (dist > 1.0) {
        discard;
    }

    // 3. Define a high-intensity neon color
    // Values over 1.0 help the "glow" look when blended
    vec3 neonCyan = vec3(0.0, 10.0, 8.0); 

    // 4. Soften the edges for a "fuzzy" spore look
    float alpha = 1.0 - dist;

    FragColor = vec4(neonCyan, alpha);
}