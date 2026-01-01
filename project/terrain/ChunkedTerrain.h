#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <unordered_map>
#include <vector>

enum class TerrainTheme { SNOW=0, GRASS=1 };

struct TerrainParams {
    float heightScale   = 180.0f;
    float baseFreq      = 0.007f;
    float detailFreq    = 0.055f;

    float ridgeStrength  = 1.1f;  // boosts peaks
    float craterStrength = 0.0f;  // bowls in crater theme

    float snowLine  = 0.55f;      // height01 threshold for snow
    float slopeRock = 0.55f;      // slope threshold for rock

    float platformRadius = 120.0f;  // button platform flatten radius
    float moatRadius     = 170.0f;  // isolation ring radius
    float moatDepth      = 120.0f;  // how deep the ring drop is
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
    // heightScale: initial height amplitude (overridden by theme presets)
    void init(float chunkWorldSize, int vertsPerSide, int radius,
              float uvTiling, float heightScale);

    void update(const glm::vec3& focusPos);
    void draw() const;

    float sampleHeightWorld(float x, float z) const;
    float sampleHeightWorldSmooth(float x, float z) const;

    void setTheme(TerrainTheme t);
    TerrainTheme theme() const { return m_theme; }
    const TerrainParams& params() const { return m_params; }

private:
    float m_chunkWorldSize = 256.0f;
    int   m_vertsPerSide   = 65;
    int   m_radius         = 2;
    float m_uvTiling       = 10.0f;

    TerrainTheme m_theme = TerrainTheme::GRASS;
    TerrainParams m_params;
    bool m_themeDirty = false;

    glm::ivec2 m_centerChunk{0,0};

    struct IVec2Hash {
        size_t operator()(const glm::ivec2& v) const noexcept {
            // decent hash
            return (size_t)(v.x * 73856093) ^ (size_t)(v.y * 19349663);
        }
    };

    std::unordered_map<glm::ivec2, TerrainChunk, IVec2Hash> m_chunks;

    glm::ivec2 worldToChunkCoord(const glm::vec3& p) const;
    void rebuildAll();
    void ensureChunksAround(const glm::ivec2& center);

    void buildChunk(TerrainChunk& c);

    // Theme height function
    float heightAt(float wx, float wz) const;

    // Used to compute normals
    float heightAtRaw(float wx, float wz) const;

    // Theme presets
    void applyThemePreset(TerrainTheme t);
};
