#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <unordered_map>
#include <vector>

struct TerrainParams {
    float heightScale   = 120.0f;    // Max terrain height
    float baseFreq      = 0.001f;    // Large rolling hills frequency
    float detailFreq    = 0.015f;    // Fine detail frequency
    float ridgeStrength = 0.1f;      // Subtle ridge lines
    float slopeRock     = 0.70f;     // Slope threshold for rock texture

    float platformRadius = 120.0f;   // Button platform flatten radius
    float moatRadius     = 170.0f;   // Isolation ring radius
    float moatDepth      = 100.0f;   // How deep the ring drop is
};

struct TerrainChunk {
    glm::ivec2 coord{0,0};   // chunk grid coordinate
    GLuint vao=0, vbo=0, ebo=0;
    int indexCount=0;
};

class ChunkedTerrain {
public:
    ChunkedTerrain() = default;
    ~ChunkedTerrain();

    // chunkWorldSize: world units per chunk
    // vertsPerSide: number of verts per side (>= 2)
    // radius: number of chunks around player (radius=2 => 5x5)
    // uvTiling: UV repeat per chunk
    // heightScale: initial height amplitude
    void init(float chunkWorldSize, int vertsPerSide, int radius,
              float uvTiling, float heightScale);

    void update(const glm::vec3& focusPos);
    void draw() const;

    float sampleHeightWorld(float x, float z) const;

    const TerrainParams& params() const { return m_params; }

private:
    float m_chunkWorldSize = 256.0f;
    int   m_vertsPerSide   = 65;
    int   m_radius         = 2;
    float m_uvTiling       = 8.0f;

    TerrainParams m_params;

    glm::ivec2 m_centerChunk{0,0};

    struct IVec2Hash {
        size_t operator()(const glm::ivec2& v) const noexcept {
            return (size_t)(v.x * 73856093) ^ (size_t)(v.y * 19349663);
        }
    };

    std::unordered_map<glm::ivec2, TerrainChunk, IVec2Hash> m_chunks;

    glm::ivec2 worldToChunkCoord(const glm::vec3& p) const;
    void ensureChunksAround(const glm::ivec2& center);

    void buildChunk(TerrainChunk& c);

    // Height generation
    float heightAt(float wx, float wz) const;
    float heightAtRaw(float wx, float wz) const;
};
