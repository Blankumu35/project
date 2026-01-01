#pragma once
#include <glad/gl.h> // FIX: Define GLuint
#include <glm/glm.hpp>
#include <vector>

struct Particle {
    glm::vec3 pos, vel;
    glm::vec4 color;
    float life;
};

class ParticleSystem {
public:
    ParticleSystem(int maxParticles);
    void update(float deltaTime, glm::vec3 playerPos);
    void draw(const glm::mat4& viewProj, GLuint program); // Correctly uses GLuint now

private:
    std::vector<Particle> m_particles;
    unsigned int m_vao, m_vbo;
};