#include "particle.h"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstdlib> // FIX: For rand() and srand()

ParticleSystem::ParticleSystem(int maxParticles) {
    m_particles.resize(maxParticles);
    for (auto& p : m_particles) {
        p.life = -1.0f; 
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, maxParticles * sizeof(glm::vec3), NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
}

void ParticleSystem::update(float deltaTime, glm::vec3 playerPos) {
    for (auto& p : m_particles) {
        if (p.life < 0) {
            // Respawn spore near player
            p.pos = playerPos + glm::vec3((std::rand() % 400) - 200, (std::rand() % 200), (std::rand() % 400) - 200);
            p.vel = glm::vec3((std::rand() % 10 - 5) * 0.1f, (std::rand() % 10) * 0.2f, (std::rand() % 10 - 5) * 0.1f);
            p.life = 5.0f + (std::rand() % 5);
        }
        p.pos += p.vel * deltaTime;
        p.life -= deltaTime;
    }
}

// FIX: Ensure this matches the GLuint in the header exactly
void ParticleSystem::draw(const glm::mat4& viewProj, GLuint program) {
    std::vector<glm::vec3> positions;
    for (const auto& p : m_particles) {
        if (p.life > 0) positions.push_back(p.pos);
    }

    if (positions.empty()) return;

glUseProgram(program);
    GLint loc = glGetUniformLocation(program, "uViewProj");
    if (loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(viewProj));
    }
    
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(glm::vec3), positions.data());
    
    glPointSize(5.0f);
    glDrawArrays(GL_POINTS, 0, (GLsizei)positions.size());
}