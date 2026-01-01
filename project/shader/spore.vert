#version 330 core

// Location 0 matches the glVertexAttribPointer setup in particle.cpp
layout (location = 0) in vec3 aPos;

// Combined Projection and View matrix from your main loop
uniform mat4 uViewProj;

void main()
{
    // Transform the world-space particle position into clip-space
    gl_Position = uViewProj * vec4(aPos, 1.0);

    // Optional: Adjust point size based on distance (attenuation)
    // Larger when close, smaller when far
    gl_PointSize = 1000.0 / gl_Position.w;
}