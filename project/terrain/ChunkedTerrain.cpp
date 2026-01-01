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
    } else if (t == TerrainTheme::GRASS) {
        m_params.heightScale = 180.0f;   // Gentle hills
        m_params.baseFreq = 0.003f;      // Very large features = vast open areas
        m_params.detailFreq = 0.025f;    // Subtle detail
        m_params.ridgeStrength = 0.3f;   // Minimal ridges = gentle slopes
        m_params.craterStrength = 0.0f;
        m_params.snowLine = 2.0f;        // No snow - impossible to reach
        m_params.slopeRock = 0.65f;
    } else { // CRATER
        m_params.heightScale = 260.0f;
        m_params.baseFreq = 0.008f;
        m_params.detailFreq = 0.085f;
        m_params.ridgeStrength = 1.1f;
        m_params.craterStrength = 1.0f;
        m_params.snowLine = 0.98f;
        m_params.slopeRock = 0.55f;
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
    // Base fBm-ish noise using perlin
    float base = glm::perlin(glm::vec2(wx * m_params.baseFreq, wz * m_params.baseFreq));
    float detail = glm::perlin(glm::vec2(wx * m_params.detailFreq + 31.3f, wz * m_params.detailFreq - 18.7f));

    // Normalize-ish perlin output [-1,1] to [0,1]
    float h = 0.5f + 0.5f * (0.70f * base + 0.30f * detail);
    
    // Flatten mid-range values to create more plains
    float flatZone = 0.5f;
    float flatRadius = 0.25f;
    float distFromFlat = std::abs(h - flatZone);
    if (distFromFlat < flatRadius) {
        float flatFactor = 1.0f - (distFromFlat / flatRadius);
        flatFactor = flatFactor * flatFactor; // ease
        h = glm::mix(h, flatZone, flatFactor * 0.7f);
    }

    // Ridge: make peaks by folding (but gentler)
    float ridge = 1.0f - std::abs(2.0f * h - 1.0f);
    ridge = ridge * ridge * ridge; // smoother peaks
    h = glm::mix(h, glm::max(h, ridge), m_params.ridgeStrength);

    // Craters: hashed grid + radial bowl
    if (m_params.craterStrength > 0.001f) {
        // Grid cell size for crater centers
        float cell = 260.0f;
        int gx = (int)std::floor(wx / cell);
        int gz = (int)std::floor(wz / cell);

        auto hash = [](int x, int z)->float {
            int n = x * 374761393 + z * 668265263;
            n = (n ^ (n >> 13)) * 1274126177;
            n = n ^ (n >> 16);
            return (n & 0x00FFFFFF) / float(0x01000000); // [0,1)
        };

        float rnd = hash(gx, gz);
        // Some cells have a crater
        if (rnd > 0.55f) {
            float cx = (gx + 0.2f + 0.6f * hash(gx+11, gz+7)) * cell;
            float cz = (gz + 0.2f + 0.6f * hash(gx+5,  gz+13)) * cell;

            float dx = wx - cx;
            float dz = wz - cz;
            float r = std::sqrt(dx*dx + dz*dz);

            float radius = 90.0f + 80.0f * hash(gx+3, gz+19);
            float sigma = radius * 0.55f;

            // Bowl
            float bowl = std::exp(-(r*r) / (2.0f * sigma * sigma));
            // Rim ring
            float rim = std::exp(-((r - radius) * (r - radius)) / (2.0f * (0.20f*radius)*(0.20f*radius)));

            // Apply: bowl subtracts height, rim adds
            h -= 0.22f * bowl * m_params.craterStrength;
            h += 0.10f * rim  * m_params.craterStrength;
        }
    }

    // Clamp and return
    return glm::clamp(h, 0.0f, 1.0f);
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
