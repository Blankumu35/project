#include "ChunkedTerrain.h"

#include <glm/gtc/noise.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/common.hpp>

#include <iostream>
#include <algorithm>
#include <cmath>

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

    m_params.heightScale = heightScale;
    applyThemePreset(m_theme);

    m_chunks.clear();
    m_centerChunk = glm::ivec2(0,0);
    ensureChunksAround(m_centerChunk);
}

glm::ivec2 ChunkedTerrain::worldToChunkCoord(const glm::vec3& p) const {
    int cx = (int)std::floor(p.x / m_chunkWorldSize);
    int cz = (int)std::floor(p.z / m_chunkWorldSize);
    return glm::ivec2(cx, cz);
}

void ChunkedTerrain::setTheme(TerrainTheme t) {
    if (t == m_theme) return;
    m_theme = t;
    applyThemePreset(t);
    m_themeDirty = true;
}

void ChunkedTerrain::applyThemePreset(TerrainTheme t) {
    if (t == TerrainTheme::SNOW) {
        m_params.heightScale = 280.0f;      // More dramatic elevation changes
        m_params.baseFreq = 0.005f;
        m_params.detailFreq = 0.030f;
        m_params.ridgeStrength = 2.2f;      // Sharper peaks for defined edges
        m_params.craterStrength = 0.15f;    // Slight bowl features for edge definition
        m_params.snowLine = 0.35f;          // Lower threshold for better contrast
        m_params.slopeRock = 0.42f;
    } else { // GRASS
        m_params.heightScale = 220.0f;      // More varied elevation
        m_params.baseFreq = 0.0015f;        // Larger base features
        m_params.detailFreq = 0.045f;       // More micro-detail
        m_params.ridgeStrength = 0.45f;     // Subtle ridges for interest
        m_params.craterStrength = 0.0f;
        m_params.snowLine = 10.0f;          // Never show snow
        m_params.slopeRock = 0.58f;         // Rock on steeper slopes
    }
}

void ChunkedTerrain::update(const glm::vec3& focusPos) {
    glm::ivec2 c = worldToChunkCoord(focusPos);
    if (c != m_centerChunk) {
        m_centerChunk = c;
        ensureChunksAround(m_centerChunk);
    }

    if (m_themeDirty) {
        rebuildAll();
        m_themeDirty = false;
    }
}

void ChunkedTerrain::rebuildAll() {
    for (auto& kv : m_chunks) {
        SafeDeleteChunkGL(kv.second);
        buildChunk(kv.second);
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

float ChunkedTerrain::heightAtRaw(float wx, float wz) const {
    // Multi-octave fBm for realistic terrain
    float amplitude = 1.0f;
    float frequency = m_params.baseFreq;
    float height = 0.0f;
    float maxValue = 0.0f;
    
    // 5 octaves for detail
    for (int i = 0; i < 5; i++) {
        // Domain warping - offset noise lookup
        float warpStrength = 30.0f;
        float wx_warped = wx + glm::perlin(glm::vec2(wx * 0.001f, wz * 0.001f)) * warpStrength;
        float wz_warped = wz + glm::perlin(glm::vec2(wx * 0.001f + 100.0f, wz * 0.001f)) * warpStrength;
        
        float noiseVal = glm::perlin(glm::vec2(wx_warped * frequency, wz_warped * frequency));
        height += noiseVal * amplitude;
        maxValue += amplitude;
        
        amplitude *= 0.5f;  // persistence
        frequency *= 2.0f;  // lacunarity
    }
    
    // Normalize to 0-1
    height = (height / maxValue) * 0.5f + 0.5f;
    
    // Ridged multifractal for mountains (theme-dependent)
    if (m_params.ridgeStrength > 0.01f) {
        float ridge = 1.0f - std::abs(glm::perlin(glm::vec2(wx * 0.003f, wz * 0.003f)));
        ridge = std::pow(ridge, 2.0f);
        height = glm::mix(height, ridge, m_params.ridgeStrength);
    }
    
    // Terracing for more interesting elevation bands
    float terraceStrength = 0.15f;
    float terraceFreq = 8.0f;
    float terraced = std::floor(height * terraceFreq) / terraceFreq;
    height = glm::mix(height, terraced, terraceStrength * height);
    
    return glm::clamp(height, 0.0f, 1.0f);
}

float ChunkedTerrain::heightAt(float wx, float wz) const {
    // Base raw
    float h01 = heightAtRaw(wx, wz);

    // Convert to world height
    float h = h01 * m_params.heightScale;

    // Center platform flatten + moat ring to isolate
    float r = std::sqrt(wx*wx + wz*wz);
    float platR = m_params.platformRadius;
    float moatR = m_params.moatRadius;

    if (r < platR) {
        // Perfect flat platform (button area)
        float platformHeight = 12.0f; // keep near ground
        h = platformHeight;
    } else if (r < moatR) {
        // Smooth drop to make the platform isolated
        float t = (r - platR) / (moatR - platR); // 0..1
        t = glm::clamp(t, 0.0f, 1.0f);
        // ease
        float e = t*t*(3.0f - 2.0f*t);
        h = glm::mix(12.0f, 12.0f - m_params.moatDepth, e);
    }

    return h;
}

float ChunkedTerrain::sampleHeightWorld(float x, float z) const {
    return heightAt(x, z);
}

float ChunkedTerrain::sampleHeightWorldSmooth(float x, float z) const {
    // Get the four corner heights of the grid cell
    float step = m_chunkWorldSize / float(m_vertsPerSide - 1);
    int ix = int(std::floor(x / step));
    int iz = int(std::floor(z / step));
    
    float h00 = heightAt(ix * step, iz * step);
    float h10 = heightAt((ix + 1) * step, iz * step);
    float h01 = heightAt(ix * step, (iz + 1) * step);
    float h11 = heightAt((ix + 1) * step, (iz + 1) * step);
    
    // Bilinear interpolation
    float fx = (x - ix * step) / step;
    float fz = (z - iz * step) / step;
    
    float h0 = glm::mix(h00, h10, fx);
    float h1 = glm::mix(h01, h11, fx);
    return glm::mix(h0, h1, fz);
}

void ChunkedTerrain::buildChunk(TerrainChunk& c) {
    // Vertex layout:
    // position (3) + normal (3) + uv (2) + height01 (1) = 9 floats
    const int stride = 9;

    const int N = m_vertsPerSide;
    const int vertsCount = N * N;
    const int cells = (N - 1) * (N - 1);

    std::vector<float> vertices;
    vertices.resize(vertsCount * stride);

    std::vector<unsigned int> indices;
    indices.resize(cells * 6);

    // Chunk origin in world
    float ox = c.coord.x * m_chunkWorldSize;
    float oz = c.coord.y * m_chunkWorldSize;

    // Step between vertices
    float step = m_chunkWorldSize / float(N - 1);

    // Fill vertices
    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            int idx = (z * N + x) * stride;

            float wx = ox + x * step;
            float wz = oz + z * step;
            float wy = heightAt(wx, wz);

            // Height01 for shading
            float h01 = glm::clamp(wy / glm::max(1.0f, m_params.heightScale), 0.0f, 1.0f);

            // Improved normal calculation using cross product method
            float eps = step;
            float hL = heightAt(wx - eps, wz);
            float hR = heightAt(wx + eps, wz);
            float hD = heightAt(wx, wz - eps);
            float hU = heightAt(wx, wz + eps);

            // Calculate tangent vectors
            glm::vec3 tangentX = glm::normalize(glm::vec3(2.0f * eps, hR - hL, 0.0f));
            glm::vec3 tangentZ = glm::normalize(glm::vec3(0.0f, hU - hD, 2.0f * eps));
            
            // Normal is cross product of tangents
            glm::vec3 n = glm::normalize(glm::cross(tangentZ, tangentX));

            // UV
            float u = (x / float(N - 1)) * m_uvTiling;
            float v = (z / float(N - 1)) * m_uvTiling;

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

    // Fill indices
    int ii = 0;
    for (int z = 0; z < N - 1; ++z) {
        for (int x = 0; x < N - 1; ++x) {
            int i0 = z * N + x;
            int i1 = i0 + 1;
            int i2 = (z + 1) * N + x;
            int i3 = i2 + 1;

            indices[ii++] = i0; indices[ii++] = i2; indices[ii++] = i1;
            indices[ii++] = i1; indices[ii++] = i2; indices[ii++] = i3;
        }
    }

    // Create buffers
    glGenVertexArrays(1, &c.vao);
    glGenBuffers(1, &c.vbo);
    glGenBuffers(1, &c.ebo);

    glBindVertexArray(c.vao);

    glBindBuffer(GL_ARRAY_BUFFER, c.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // aPos
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

    c.indexCount = (int)indices.size();
}

void ChunkedTerrain::draw() const {
    for (const auto& kv : m_chunks) {
        const TerrainChunk& c = kv.second;
        if (!c.vao || c.indexCount <= 0) continue;
        glBindVertexArray(c.vao);
        glDrawElements(GL_TRIANGLES, c.indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}
