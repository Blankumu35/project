#include "ChunkedTerrain.h"

#include <glm/gtc/noise.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/common.hpp>

#include <iostream>
#include <algorithm>
#include <cmath>

// ============================================================================
// WONDERLAND GRASSY TERRAIN - Fresh Implementation
// ============================================================================

static void SafeDeleteChunkGL(TerrainChunk& c) {
    if (c.ebo) glDeleteBuffers(1, &c.ebo);
    if (c.vbo) glDeleteBuffers(1, &c.vbo);
    if (c.vao) glDeleteVertexArrays(1, &c.vao);
    c.ebo = c.vbo = c.vao = 0;
    c.indexCount = 0;
}

ChunkedTerrain::~ChunkedTerrain() {
    for (auto& kv : m_chunks) SafeDeleteChunkGL(kv.second);
    m_chunks.clear();
}

void ChunkedTerrain::init(float chunkWorldSize, int vertsPerSide, int radius,
                          float uvTiling, float heightScale) {
    m_chunkWorldSize = chunkWorldSize;
    m_vertsPerSide   = std::max(2, vertsPerSide);
    m_radius         = std::max(1, radius);
    m_uvTiling       = uvTiling;

    // Wonderland grassy terrain parameters
    m_params.heightScale   = 100.0f;    // Gentle rolling hills
    m_params.baseFreq      = 0.0006f;   // Very large, sweeping features
    m_params.detailFreq    = 0.012f;    // Soft micro-detail
    m_params.ridgeStrength = 0.08f;     // Very subtle ridges
    m_params.slopeRock     = 0.72f;     // Rock only on steep slopes

    m_chunks.clear();
    m_centerChunk = glm::ivec2(0,0);
    ensureChunksAround(m_centerChunk);
}

glm::ivec2 ChunkedTerrain::worldToChunkCoord(const glm::vec3& p) const {
    int cx = (int)std::floor(p.x / m_chunkWorldSize);
    int cz = (int)std::floor(p.z / m_chunkWorldSize);
    return glm::ivec2(cx, cz);
}

void ChunkedTerrain::update(const glm::vec3& focusPos) {
    glm::ivec2 c = worldToChunkCoord(focusPos);
    if (c != m_centerChunk) {
        m_centerChunk = c;
        ensureChunksAround(m_centerChunk);
    }
}

void ChunkedTerrain::ensureChunksAround(const glm::ivec2& center) {
    // Remove chunks outside radius
    std::vector<glm::ivec2> toRemove;
    toRemove.reserve(m_chunks.size());
    for (auto& kv : m_chunks) {
        glm::ivec2 cc = kv.first;
        if (std::abs(cc.x - center.x) > m_radius || std::abs(cc.y - center.y) > m_radius) {
            toRemove.push_back(cc);
        }
    }
    for (auto& k : toRemove) {
        SafeDeleteChunkGL(m_chunks[k]);
        m_chunks.erase(k);
    }

    // Add needed chunks
    for (int dz = -m_radius; dz <= m_radius; ++dz) {
        for (int dx = -m_radius; dx <= m_radius; ++dx) {
            glm::ivec2 key(center.x + dx, center.y + dz);
            if (m_chunks.find(key) == m_chunks.end()) {
                TerrainChunk c;
                c.coord = key;
                buildChunk(c);
                m_chunks.emplace(key, c);
            }
        }
    }
}

// ============================================================================
// WONDERLAND HEIGHT GENERATION
// Organic, dreamy rolling meadows with gentle hills
// ============================================================================

float ChunkedTerrain::heightAtRaw(float wx, float wz) const {
    float height = 0.0f;
    float amplitude = 1.0f;
    float frequency = m_params.baseFreq;
    float maxValue = 0.0f;
    
    // 5 octaves of smooth, organic noise
    for (int i = 0; i < 5; i++) {
        // Gentle domain warping for organic flow
        float warpAmount = 40.0f / (1.0f + float(i));
        float warpX = glm::perlin(glm::vec2(wx * 0.0003f, wz * 0.0003f)) * warpAmount;
        float warpZ = glm::perlin(glm::vec2(wx * 0.0003f + 100.0f, wz * 0.0003f)) * warpAmount;
        
        float nx = (wx + warpX) * frequency;
        float nz = (wz + warpZ) * frequency;
        
        float n = glm::perlin(glm::vec2(nx, nz));
        height += n * amplitude;
        maxValue += amplitude;
        
        amplitude *= 0.42f;  // Smooth persistence for rolling hills
        frequency *= 2.2f;
    }
    
    // Normalize to 0-1 range
    height = (height / maxValue) * 0.5f + 0.5f;
    
    // Add gentle billowy hills
    float billow1 = std::abs(glm::perlin(glm::vec2(wx * 0.0015f, wz * 0.0015f)));
    float billow2 = std::abs(glm::perlin(glm::vec2(wx * 0.0025f + 50.0f, wz * 0.002f)));
    float billowBlend = billow1 * 0.6f + billow2 * 0.4f;
    billowBlend = std::pow(billowBlend, 0.8f);  // Soften peaks
    
    height = glm::mix(height, billowBlend * 0.7f + 0.3f, 0.35f);
    
    // Very subtle ridge lines for interest
    if (m_params.ridgeStrength > 0.01f) {
        float ridge = 1.0f - std::abs(glm::perlin(glm::vec2(wx * 0.0008f, wz * 0.0008f)));
        ridge = std::pow(ridge, 4.0f);  // Very soft ridges
        height = glm::mix(height, height + ridge * 0.15f, m_params.ridgeStrength);
    }
    
    // Gentle sinusoidal waves for meadow undulation
    float wave1 = std::sin(wx * 0.002f + wz * 0.0015f) * 0.5f + 0.5f;
    float wave2 = std::sin(wx * 0.0015f - wz * 0.0025f + 2.0f) * 0.5f + 0.5f;
    height = glm::mix(height, (wave1 + wave2) * 0.5f, 0.12f);
    
    // Smooth valleys - no harsh transitions
    height = glm::smoothstep(0.0f, 1.0f, height);
    
    return glm::clamp(height, 0.0f, 1.0f);
}

float ChunkedTerrain::heightAt(float wx, float wz) const {
    float h01 = heightAtRaw(wx, wz);
    float h = h01 * m_params.heightScale;

    // Center platform flatten + isolation moat
    float r = std::sqrt(wx*wx + wz*wz);
    float platR = m_params.platformRadius;
    float moatR = m_params.moatRadius;

    if (r < platR) {
        // Flat platform for button area
        h = 10.0f;
    } else if (r < moatR) {
        // Smooth drop creating isolated platform
        float t = (r - platR) / (moatR - platR);
        t = glm::clamp(t, 0.0f, 1.0f);
        float e = t * t * (3.0f - 2.0f * t);  // smoothstep
        h = glm::mix(10.0f, 10.0f - m_params.moatDepth, e);
    }

    return h;
}

float ChunkedTerrain::sampleHeightWorld(float x, float z) const {
    return heightAt(x, z);
}

// ============================================================================
// CHUNK MESH BUILDING
// ============================================================================

void ChunkedTerrain::buildChunk(TerrainChunk& c) {
    // Vertex layout: position(3) + normal(3) + uv(2) + height01(1) = 9 floats
    const int stride = 9;
    const int N = m_vertsPerSide;
    const int vertsCount = N * N;
    const int cells = (N - 1) * (N - 1);

    std::vector<float> vertices(vertsCount * stride);
    std::vector<unsigned int> indices(cells * 6);

    // Chunk origin in world space
    float ox = c.coord.x * m_chunkWorldSize;
    float oz = c.coord.y * m_chunkWorldSize;
    float step = m_chunkWorldSize / float(N - 1);

    // Generate vertices
    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            int idx = (z * N + x) * stride;

            float wx = ox + x * step;
            float wz = oz + z * step;
            float wy = heightAt(wx, wz);

            // Normalized height for shader
            float h01 = glm::clamp(wy / glm::max(1.0f, m_params.heightScale), 0.0f, 1.0f);

            // Calculate normal using cross product of tangents
            float eps = step;
            float hL = heightAt(wx - eps, wz);
            float hR = heightAt(wx + eps, wz);
            float hD = heightAt(wx, wz - eps);
            float hU = heightAt(wx, wz + eps);

            glm::vec3 tangentX = glm::normalize(glm::vec3(2.0f * eps, hR - hL, 0.0f));
            glm::vec3 tangentZ = glm::normalize(glm::vec3(0.0f, hU - hD, 2.0f * eps));
            glm::vec3 n = glm::normalize(glm::cross(tangentZ, tangentX));

            // UV coordinates
            float u = (x / float(N - 1)) * m_uvTiling;
            float v = (z / float(N - 1)) * m_uvTiling;

            // Store vertex data
            vertices[idx + 0] = wx;
            vertices[idx + 1] = wy;
            vertices[idx + 2] = wz;
            vertices[idx + 3] = n.x;
            vertices[idx + 4] = n.y;
            vertices[idx + 5] = n.z;
            vertices[idx + 6] = u;
            vertices[idx + 7] = v;
            vertices[idx + 8] = h01;
        }
    }

    // Generate indices
    int ii = 0;
    for (int z = 0; z < N - 1; ++z) {
        for (int x = 0; x < N - 1; ++x) {
            int tl = z * N + x;
            int tr = tl + 1;
            int bl = tl + N;
            int br = bl + 1;

            indices[ii++] = tl;
            indices[ii++] = bl;
            indices[ii++] = tr;
            indices[ii++] = tr;
            indices[ii++] = bl;
            indices[ii++] = br;
        }
    }

    c.indexCount = (int)indices.size();

    // Upload to GPU
    glGenVertexArrays(1, &c.vao);
    glGenBuffers(1, &c.vbo);
    glGenBuffers(1, &c.ebo);

    glBindVertexArray(c.vao);

    glBindBuffer(GL_ARRAY_BUFFER, c.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Vertex attributes
    // aPosition
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)0);
    // aNormal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(3 * sizeof(float)));
    // aUV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(6 * sizeof(float)));
    // aHeight01
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(8 * sizeof(float)));

    glBindVertexArray(0);
}

void ChunkedTerrain::draw() const {
    for (auto& kv : m_chunks) {
        const TerrainChunk& c = kv.second;
        if (c.vao && c.indexCount > 0) {
            glBindVertexArray(c.vao);
            glDrawElements(GL_TRIANGLES, c.indexCount, GL_UNSIGNED_INT, nullptr);
        }
    }
    glBindVertexArray(0);
}
