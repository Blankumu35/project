#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <tiny_gltf.h>
#include <vector>
#include <string>
#include <map>

// Animation state machine
enum class BotAnimState {
    IDLE,
    WALK,
    RUN,
    JUMP
};

class Bot {
public:
    Bot();
    ~Bot();
    
    void initialize(const std::string& modelPath);
    void update(float deltaTime, const glm::vec3& moveInput, bool jumpPressed, float terrainHeight);
    void render(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& lightPos, const glm::vec3& lightIntensity);
    
    glm::vec3 getPosition() const { return m_position; }
    void setPosition(const glm::vec3& pos) { m_position = pos; }
    
    glm::vec3 getForward() const { return m_forward; }
    float getRotationY() const { return m_rotationY; }
    
private:
    // Model data structures
    struct PrimitiveObject {
        GLuint vao;
        std::map<int, GLuint> vbos;
    };
    
    struct SkinObject {
        std::vector<glm::mat4> inverseBindMatrices;
        std::vector<glm::mat4> globalJointTransforms;
        std::vector<glm::mat4> jointMatrices;
    };
    
    struct SamplerObject {
        std::vector<float> input;
        std::vector<glm::vec4> output;
        int interpolation;
    };
    
    struct ChannelObject {
        int sampler;
        std::string targetPath;
        int targetNode;
    };
    
    struct AnimationObject {
        std::vector<SamplerObject> samplers;
    };
    
    // Model data
    tinygltf::Model m_model;
    std::vector<PrimitiveObject> m_primitives;
    std::vector<SkinObject> m_skins;
    std::vector<AnimationObject> m_animations;
    
    GLuint m_programID;
    GLuint m_mvpMatrixID, m_lightPosID, m_lightIntensityID, m_jointMatricesID;
    
    // Transform
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};
    glm::vec3 m_forward{0.0f, 0.0f, -1.0f};
    float m_rotationY = 0.0f;
    float m_scale = 15.0f;
    
    // Physics
    glm::vec3 m_velocity{0.0f};
    float m_verticalVelocity = 0.0f;
    bool m_isGrounded = false;
    
    // Animation
    BotAnimState m_currentState = BotAnimState::IDLE;
    float m_animTime = 0.0f;
    int m_currentAnimIndex = 0;
    
    // Constants
    const float WALK_SPEED = 85.0f;
    const float RUN_SPEED = 180.0f;
    const float JUMP_FORCE = 320.0f;
    const float GRAVITY = -980.0f;
    const float TURN_SPEED = 5.0f;
    const float COLLISION_RADIUS = 18.0f;
    const float COLLISION_HEIGHT = 40.0f;
    
    // Helper methods
    void updateAnimation(float deltaTime);
    void updatePhysics(float deltaTime, float terrainHeight);
    void updateMovement(const glm::vec3& input, float deltaTime);
    BotAnimState determineAnimState(const glm::vec3& input, bool jumpPressed);
    
    // GLTF loading and rendering helpers
    bool loadModel(const char* filename);
    std::vector<PrimitiveObject> bindModel(tinygltf::Model& model);
    void bindModelNodes(std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model& model, tinygltf::Node& node);
    void bindMesh(std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model& model, tinygltf::Mesh& mesh);
    
    void drawModel(const std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model& model);
    void drawModelNodes(const std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model& model, tinygltf::Node& node);
    void drawMesh(const std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model& model, tinygltf::Mesh& mesh);
    
    // Animation helpers
    std::vector<SkinObject> prepareSkinning(const tinygltf::Model& model);
    std::vector<AnimationObject> prepareAnimation(const tinygltf::Model& model);
    void updateSkinning(const std::vector<glm::mat4>& nodeTransforms);
    void updateAnimationFrame(const tinygltf::Model& model, const tinygltf::Animation& anim, 
                              const AnimationObject& animationObject, float time, 
                              std::vector<glm::mat4>& nodeTransforms);
    
    // Transform helpers
    glm::mat4 getNodeTransform(const tinygltf::Node& node);
    void computeLocalNodeTransform(const tinygltf::Model& model, int nodeIndex, std::vector<glm::mat4>& localTransforms);
    void computeGlobalNodeTransform(const tinygltf::Model& model, const std::vector<glm::mat4>& localTransforms,
                                    int nodeIndex, const glm::mat4& parentTransform, std::vector<glm::mat4>& globalTransforms);
    int findKeyframeIndex(const std::vector<float>& times, float animationTime);
};
