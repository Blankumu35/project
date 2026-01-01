#version 330 core
layout(location = 0) in vec3 vertexPosition; // Quad vertices
// We will use uniforms to move the particles for simplicity in this project
uniform mat4 MVP;
uniform vec4 particleColor;

out vec4 Color;

void main() {
    gl_Position = MVP * vec4(vertexPosition, 1.0);
    Color = particleColor;
}