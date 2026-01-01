#include "Bot.h"
#include <render/shader.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <cmath>

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

Bot::Bot() {
}

Bot::~Bot() {
    // Cleanup OpenGL resources
    if (m_programID) {
        glDeleteProgram(m_programID);
    }
    for (auto& prim : m_primitives) {
        if (prim.vao) glDeleteVertexArrays(1, &prim.vao);
        for (auto& vbo : prim.vbos) {
            glDeleteBuffers(1, &vbo.second);
        }
    }
}

bool Bot::loadModel(const char* filename) {
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    
    bool res = loader.LoadASCIIFromFile(&m_model, &err, &warn, filename);
    if (!warn.empty()) {
        std::cout << "WARN: " << warn << std::endl;
    }
    
    if (!err.empty()) {
        std::cout << "ERR: " << err << std::endl;
    }
    
    if (!res) {
        std::cout << "Failed to load glTF: " << filename << std::endl;
    } else {
        std::cout << "Loaded glTF: " << filename << std::endl;
    }
    
    return res;
}

void Bot::initialize(const std::string& modelPath) {
    // Load model
    if (!loadModel(modelPath.c_str())) {
        return;
    }
    
    // Prepare buffers for rendering
    m_primitives = bindModel(m_model);
    
    // Prepare joint matrices
    m_skins = prepareSkinning(m_model);
    
    // Prepare animation data
    m_animations = prepareAnimation(m_model);
    
    // Load shaders
    m_programID = LoadShadersFromFile("../project/shader/bot.vert", "../project/shader/bot.frag");
    if (m_programID == 0) {
        std::cerr << "Failed to load bot shaders." << std::endl;
    }
    
    // Get uniform locations
    m_mvpMatrixID = glGetUniformLocation(m_programID, "MVP");
    m_lightPosID = glGetUniformLocation(m_programID, "lightPosition");
    m_lightIntensityID = glGetUniformLocation(m_programID, "lightIntensity");
    m_jointMatricesID = glGetUniformLocation(m_programID, "jointMatrices");
}

void Bot::update(float deltaTime, const glm::vec3& moveInput, bool jumpPressed, float terrainHeight) {
    // Update movement and rotation
    updateMovement(moveInput, deltaTime);
    
    // Update physics (gravity, jumping)
    updatePhysics(deltaTime, terrainHeight);
    
    // Determine animation state
    m_currentState = determineAnimState(moveInput, jumpPressed);
    
    // Update animation
    updateAnimation(deltaTime);
}

void Bot::updateMovement(const glm::vec3& input, float deltaTime) {
    if (glm::length(input) > 0.01f) {
        // Determine target rotation from input
        float targetAngle = std::atan2(input.x, input.z);
        
        // Smooth rotation towards target
        float angleDiff = targetAngle - m_rotationY;
        
        // Normalize angle difference to [-pi, pi]
        while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
        while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;
        
        // Apply rotation
        m_rotationY += angleDiff * TURN_SPEED * deltaTime;
        
        // Update forward vector
        m_forward.x = std::sin(m_rotationY);
        m_forward.z = std::cos(m_rotationY);
        
        // Move in forward direction
        float speed = (glm::length(input) > 1.5f) ? RUN_SPEED : WALK_SPEED;
        glm::vec3 moveDir = glm::normalize(glm::vec3(input.x, 0.0f, input.z));
        m_position += moveDir * speed * deltaTime;
    }
}

void Bot::updatePhysics(float deltaTime, float terrainHeight) {
    // Apply gravity
    m_verticalVelocity += GRAVITY * deltaTime;
    m_position.y += m_verticalVelocity * deltaTime;
    
    // Ground collision
    float groundY = terrainHeight + COLLISION_HEIGHT * 0.5f;
    if (m_position.y <= groundY) {
        m_position.y = groundY;
        m_verticalVelocity = 0.0f;
        m_isGrounded = true;
    } else {
        m_isGrounded = false;
    }
}

BotAnimState Bot::determineAnimState(const glm::vec3& input, bool jumpPressed) {
    // Jump takes priority
    if (jumpPressed && m_isGrounded) {
        m_verticalVelocity = JUMP_FORCE;
        return BotAnimState::JUMP;
    }
    
    if (!m_isGrounded) {
        return BotAnimState::JUMP;
    }
    
    // Check movement
    float inputMag = glm::length(input);
    if (inputMag > 1.5f) {
        return BotAnimState::RUN;
    } else if (inputMag > 0.01f) {
        return BotAnimState::WALK;
    }
    
    return BotAnimState::IDLE;
}

void Bot::updateAnimation(float deltaTime) {
    if (m_model.animations.empty() || m_model.skins.empty()) {
        return;
    }
    
    // Adjust animation speed based on state
    float speedMultiplier = 1.0f;
    switch (m_currentState) {
        case BotAnimState::IDLE: speedMultiplier = 1.0f; break;
        case BotAnimState::WALK: speedMultiplier = 1.5f; break;
        case BotAnimState::RUN: speedMultiplier = 2.5f; break;
        case BotAnimState::JUMP: speedMultiplier = 1.0f; break;
    }
    
    m_animTime += deltaTime * speedMultiplier;
    
    // Use first animation (ideally we'd select based on state)
    const tinygltf::Animation& animation = m_model.animations[0];
    const AnimationObject& animationObject = m_animations[0];
    
    // Local transforms for all nodes this frame
    std::vector<glm::mat4> localNodeTransforms(m_model.nodes.size(), glm::mat4(1.0f));
    
    // Apply animated TRS into localNodeTransforms
    updateAnimationFrame(m_model, animation, animationObject, m_animTime, localNodeTransforms);
    
    // Compute global transforms from scene roots
    std::vector<glm::mat4> globalNodeTransforms(m_model.nodes.size(), glm::mat4(1.0f));
    const tinygltf::Scene& scene = m_model.scenes[m_model.defaultScene];
    glm::mat4 identity(1.0f);
    
    for (size_t r = 0; r < scene.nodes.size(); ++r) {
        int rootIndex = scene.nodes[r];
        computeGlobalNodeTransform(m_model, localNodeTransforms, rootIndex, identity, globalNodeTransforms);
    }
    
    // Feed those global transforms into skinning
    updateSkinning(globalNodeTransforms);
}

void Bot::render(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& lightPos, const glm::vec3& lightIntensity) {
    glUseProgram(m_programID);
    
    // Build model matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);
    model = glm::rotate(model, m_rotationY, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(m_scale));
    
    // MVP matrix
    glm::mat4 mvp = proj * view * model;
    glUniformMatrix4fv(m_mvpMatrixID, 1, GL_FALSE, glm::value_ptr(mvp));
    
    // Send joint matrices for skinning
    if (!m_skins.empty() && m_jointMatricesID != -1) {
        const SkinObject& skinObject = m_skins[0];
        if (!skinObject.jointMatrices.empty()) {
            glUniformMatrix4fv(
                m_jointMatricesID,
                static_cast<GLsizei>(skinObject.jointMatrices.size()),
                GL_FALSE,
                glm::value_ptr(skinObject.jointMatrices[0])
            );
        }
    }
    
    // Set light data
    glUniform3fv(m_lightPosID, 1, glm::value_ptr(lightPos));
    glUniform3fv(m_lightIntensityID, 1, glm::value_ptr(lightIntensity));
    
    // Draw the model
    drawModel(m_primitives, m_model);
}

// ============================================================================
// GLTF Loading and Binding
// ============================================================================

void Bot::bindMesh(std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model& model, tinygltf::Mesh& mesh) {
    std::map<int, GLuint> vbos;
    for (size_t i = 0; i < model.bufferViews.size(); ++i) {
        const tinygltf::BufferView& bufferView = model.bufferViews[i];
        
        if (bufferView.target == 0) {
            continue;  // Skip skinning weights buffer
        }
        
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(bufferView.target, vbo);
        glBufferData(bufferView.target, bufferView.byteLength,
                    &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
        
        vbos[i] = vbo;
    }
    
    // Bind each primitive
    for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        tinygltf::Primitive primitive = mesh.primitives[i];
        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];
        
        GLuint vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        
        for (auto& attrib : primitive.attributes) {
            tinygltf::Accessor accessor = model.accessors[attrib.second];
            int byteStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
            glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);
            
            int size = 1;
            if (accessor.type != TINYGLTF_TYPE_SCALAR) {
                size = accessor.type;
            }
            
            int vaa = -1;
            if (attrib.first.compare("POSITION") == 0) vaa = 0;
            if (attrib.first.compare("NORMAL") == 0) vaa = 1;
            if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
            if (attrib.first.compare("JOINTS_0") == 0) vaa = 3;
            if (attrib.first.compare("WEIGHTS_0") == 0) vaa = 4;
            
            if (vaa > -1) {
                glEnableVertexAttribArray(vaa);
                glVertexAttribPointer(vaa, size, accessor.componentType,
                                    accessor.normalized ? GL_TRUE : GL_FALSE,
                                    byteStride, BUFFER_OFFSET(accessor.byteOffset));
            }
        }
        
        PrimitiveObject primitiveObject;
        primitiveObject.vao = vao;
        primitiveObject.vbos = vbos;
        primitiveObjects.push_back(primitiveObject);
        
        glBindVertexArray(0);
    }
}

void Bot::bindModelNodes(std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model& model, tinygltf::Node& node) {
    if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
        bindMesh(primitiveObjects, model, model.meshes[node.mesh]);
    }
    
    for (size_t i = 0; i < node.children.size(); i++) {
        bindModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
    }
}

std::vector<Bot::PrimitiveObject> Bot::bindModel(tinygltf::Model& model) {
    std::vector<PrimitiveObject> primitiveObjects;
    
    const tinygltf::Scene& scene = model.scenes[model.defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        bindModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
    }
    
    return primitiveObjects;
}

// ============================================================================
// Drawing
// ============================================================================

void Bot::drawMesh(const std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model& model, tinygltf::Mesh& mesh) {
    for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        GLuint vao = primitiveObjects[i].vao;
        std::map<int, GLuint> vbos = primitiveObjects[i].vbos;
        
        glBindVertexArray(vao);
        
        tinygltf::Primitive primitive = mesh.primitives[i];
        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));
        
        glDrawElements(primitive.mode, indexAccessor.count,
                      indexAccessor.componentType,
                      BUFFER_OFFSET(indexAccessor.byteOffset));
        
        glBindVertexArray(0);
    }
}

void Bot::drawModelNodes(const std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model& model, tinygltf::Node& node) {
    if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
        drawMesh(primitiveObjects, model, model.meshes[node.mesh]);
    }
    for (size_t i = 0; i < node.children.size(); i++) {
        drawModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
    }
}

void Bot::drawModel(const std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model& model) {
    const tinygltf::Scene& scene = model.scenes[model.defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        drawModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
    }
}

// ============================================================================
// Animation and Skinning
// ============================================================================

glm::mat4 Bot::getNodeTransform(const tinygltf::Node& node) {
    glm::mat4 transform(1.0f);
    
    if (node.matrix.size() == 16) {
        transform = glm::make_mat4(node.matrix.data());
    } else {
        if (node.translation.size() == 3) {
            transform = glm::translate(transform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
        }
        if (node.rotation.size() == 4) {
            glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            transform *= glm::mat4_cast(q);
        }
        if (node.scale.size() == 3) {
            transform = glm::scale(transform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
        }
    }
    return transform;
}

void Bot::computeLocalNodeTransform(const tinygltf::Model& model, int nodeIndex, std::vector<glm::mat4>& localTransforms) {
    const tinygltf::Node& node = model.nodes[nodeIndex];
    localTransforms[nodeIndex] = getNodeTransform(node);
    
    for (int childIndex : node.children) {
        computeLocalNodeTransform(model, childIndex, localTransforms);
    }
}

void Bot::computeGlobalNodeTransform(const tinygltf::Model& model, const std::vector<glm::mat4>& localTransforms,
                                     int nodeIndex, const glm::mat4& parentTransform, std::vector<glm::mat4>& globalTransforms) {
    glm::mat4 global = parentTransform * localTransforms[nodeIndex];
    globalTransforms[nodeIndex] = global;
    
    const tinygltf::Node& node = model.nodes[nodeIndex];
    for (int childIndex : node.children) {
        computeGlobalNodeTransform(model, localTransforms, childIndex, global, globalTransforms);
    }
}

std::vector<Bot::SkinObject> Bot::prepareSkinning(const tinygltf::Model& model) {
    std::vector<SkinObject> skinObjects;
    
    for (size_t i = 0; i < model.skins.size(); i++) {
        SkinObject skinObject;
        
        const tinygltf::Skin& skin = model.skins[i];
        
        // Read inverseBindMatrices
        const tinygltf::Accessor& accessor = model.accessors[skin.inverseBindMatrices];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        const float* ptr = reinterpret_cast<const float*>(
            buffer.data.data() + accessor.byteOffset + bufferView.byteOffset);
        
        skinObject.inverseBindMatrices.resize(accessor.count);
        for (size_t j = 0; j < accessor.count; j++) {
            float m[16];
            memcpy(m, ptr + j * 16, 16 * sizeof(float));
            skinObject.inverseBindMatrices[j] = glm::make_mat4(m);
        }
        
        skinObject.globalJointTransforms.resize(skin.joints.size());
        skinObject.jointMatrices.resize(skin.joints.size());
        
        // Compute global transforms in T-pose
        std::vector<glm::mat4> localNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
        std::vector<glm::mat4> globalNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
        
        const tinygltf::Scene& scene = model.scenes[model.defaultScene];
        for (size_t r = 0; r < scene.nodes.size(); ++r) {
            int rootIndex = scene.nodes[r];
            computeLocalNodeTransform(model, rootIndex, localNodeTransforms);
        }
        
        glm::mat4 identity(1.0f);
        for (size_t r = 0; r < scene.nodes.size(); ++r) {
            int rootIndex = scene.nodes[r];
            computeGlobalNodeTransform(model, localNodeTransforms, rootIndex, identity, globalNodeTransforms);
        }
        
        for (size_t j = 0; j < skin.joints.size(); ++j) {
            int nodeIndex = skin.joints[j];
            glm::mat4 globalJoint = globalNodeTransforms[nodeIndex];
            skinObject.globalJointTransforms[j] = globalJoint;
            skinObject.jointMatrices[j] = globalJoint * skinObject.inverseBindMatrices[j];
        }
        
        skinObjects.push_back(skinObject);
    }
    return skinObjects;
}

int Bot::findKeyframeIndex(const std::vector<float>& times, float animationTime) {
    int left = 0;
    int right = times.size() - 1;
    
    while (left <= right) {
        int mid = (left + right) / 2;
        
        if (mid + 1 < times.size() && times[mid] <= animationTime && animationTime < times[mid + 1]) {
            return mid;
        } else if (times[mid] > animationTime) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    
    return times.size() - 2;
}

std::vector<Bot::AnimationObject> Bot::prepareAnimation(const tinygltf::Model& model) {
    std::vector<AnimationObject> animationObjects;
    for (const auto& anim : model.animations) {
        AnimationObject animationObject;
        
        for (const auto& sampler : anim.samplers) {
            SamplerObject samplerObject;
            
            const tinygltf::Accessor& inputAccessor = model.accessors[sampler.input];
            const tinygltf::BufferView& inputBufferView = model.bufferViews[inputAccessor.bufferView];
            const tinygltf::Buffer& inputBuffer = model.buffers[inputBufferView.buffer];
            
            samplerObject.input.resize(inputAccessor.count);
            
            const unsigned char* inputPtr = &inputBuffer.data[inputBufferView.byteOffset + inputAccessor.byteOffset];
            int stride = inputAccessor.ByteStride(inputBufferView);
            for (size_t i = 0; i < inputAccessor.count; ++i) {
                samplerObject.input[i] = *reinterpret_cast<const float*>(inputPtr + i * stride);
            }
            
            const tinygltf::Accessor& outputAccessor = model.accessors[sampler.output];
            const tinygltf::BufferView& outputBufferView = model.bufferViews[outputAccessor.bufferView];
            const tinygltf::Buffer& outputBuffer = model.buffers[outputBufferView.buffer];
            
            const unsigned char* outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
            
            samplerObject.output.resize(outputAccessor.count);
            
            for (size_t i = 0; i < outputAccessor.count; ++i) {
                if (outputAccessor.type == TINYGLTF_TYPE_VEC3) {
                    memcpy(&samplerObject.output[i], outputPtr + i * 3 * sizeof(float), 3 * sizeof(float));
                } else if (outputAccessor.type == TINYGLTF_TYPE_VEC4) {
                    memcpy(&samplerObject.output[i], outputPtr + i * 4 * sizeof(float), 4 * sizeof(float));
                }
            }
            
            animationObject.samplers.push_back(samplerObject);
        }
        
        animationObjects.push_back(animationObject);
    }
    return animationObjects;
}

void Bot::updateAnimationFrame(const tinygltf::Model& model, const tinygltf::Animation& anim,
                               const AnimationObject& animationObject, float time,
                               std::vector<glm::mat4>& nodeTransforms) {
    for (const auto& channel : anim.channels) {
        int targetNodeIndex = channel.target_node;
        const auto& sampler = anim.samplers[channel.sampler];
        
        const tinygltf::Accessor& outputAccessor = model.accessors[sampler.output];
        const tinygltf::BufferView& outputBufferView = model.bufferViews[outputAccessor.bufferView];
        const tinygltf::Buffer& outputBuffer = model.buffers[outputBufferView.buffer];
        
        const std::vector<float>& times = animationObject.samplers[channel.sampler].input;
        float animationTime = fmod(time, times.back());
        
        int keyframeIndex = findKeyframeIndex(times, animationTime);
        int nextIndex = keyframeIndex + 1;
        if (nextIndex >= static_cast<int>(times.size())) {
            nextIndex = keyframeIndex;
        }
        
        float t0 = times[keyframeIndex];
        float t1 = times[nextIndex];
        float factor = 0.0f;
        if (t1 > t0) {
            factor = (animationTime - t0) / (t1 - t0);
            factor = glm::clamp(factor, 0.0f, 1.0f);
        }
        
        const unsigned char* outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
        
        if (channel.target_path == "translation") {
            glm::vec3 translation0, translation1;
            memcpy(&translation0, outputPtr + keyframeIndex * 3 * sizeof(float), 3 * sizeof(float));
            memcpy(&translation1, outputPtr + nextIndex * 3 * sizeof(float), 3 * sizeof(float));
            
            glm::vec3 translation = glm::mix(translation0, translation1, factor);
            nodeTransforms[targetNodeIndex] = glm::translate(nodeTransforms[targetNodeIndex], translation);
            
        } else if (channel.target_path == "rotation") {
            glm::quat rotation0, rotation1;
            memcpy(&rotation0, outputPtr + keyframeIndex * 4 * sizeof(float), 4 * sizeof(float));
            memcpy(&rotation1, outputPtr + nextIndex * 4 * sizeof(float), 4 * sizeof(float));
            
            glm::quat rotation = glm::normalize(glm::slerp(rotation0, rotation1, factor));
            nodeTransforms[targetNodeIndex] *= glm::mat4_cast(rotation);
            
        } else if (channel.target_path == "scale") {
            glm::vec3 scale0, scale1;
            memcpy(&scale0, outputPtr + keyframeIndex * 3 * sizeof(float), 3 * sizeof(float));
            memcpy(&scale1, outputPtr + nextIndex * 3 * sizeof(float), 3 * sizeof(float));
            
            glm::vec3 scale = glm::mix(scale0, scale1, factor);
            nodeTransforms[targetNodeIndex] = glm::scale(nodeTransforms[targetNodeIndex], scale);
        }
    }
}

void Bot::updateSkinning(const std::vector<glm::mat4>& nodeTransforms) {
    if (m_model.skins.empty() || m_skins.empty()) return;
    
    for (size_t s = 0; s < m_model.skins.size(); ++s) {
        const tinygltf::Skin& skin = m_model.skins[s];
        SkinObject& skinObject = m_skins[s];
        
        for (size_t j = 0; j < skin.joints.size(); ++j) {
            int nodeIndex = skin.joints[j];
            glm::mat4 globalJoint = nodeTransforms[nodeIndex];
            skinObject.globalJointTransforms[j] = globalJoint;
            skinObject.jointMatrices[j] = globalJoint * skinObject.inverseBindMatrices[j];
        }
    }
}
