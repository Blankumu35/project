// ============================================================================
// ENDLESS FREEFALL - Character falling through infinite sky
// ============================================================================

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

// GLTF model loader
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <render/shader.h>

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <map>

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------
static GLFWwindow* window = nullptr;
static int windowWidth = 1280;
static int windowHeight = 720;

// ---------------------------------------------------------------------------
// Character state
// ---------------------------------------------------------------------------
static glm::vec3 characterPos(0.0f, 0.0f, 0.0f);
static glm::vec3 characterVel(0.0f, -50.0f, 0.0f);  // Falling velocity
static float characterRotX = 0.0f;  // Tumbling rotation
static float characterRotZ = 0.0f;
static float tumbleSpeedX = 45.0f;   // Degrees per second
static float tumbleSpeedZ = 30.0f;
static float characterFacingAngle = 0.0f;  // Y-axis rotation (facing direction)
static float targetFacingAngle = 0.0f;    // Target angle to smoothly rotate to
static bool isMoving = false;              // Whether character is actively moving

// Wind drift
static float windTime = 0.0f;

// World state (0 = day, 1 = night, 2 = chrome, 3 = speedforce, 4 = hall of mirrors)
static int currentWorld = 0;
static float worldTransition = 0.0f;  // For smooth transition effect
static float portalCooldown = 0.0f;   // Prevent instant re-entry
static float timeInWorld = 0.0f;      // Time spent in current world (portals appear after 7 seconds)

// Second portal (to chrome world)
static float portal2Y = -80.0f;  // Second portal position

// Third portal (to speedforce)
static float portal3Y = -120.0f;

// Fourth portal (to hall of mirrors)
static float portal4Y = -160.0f;

// Mirror plane data for Hall of Mirrors world
static GLuint mirrorVAO = 0, mirrorVBO = 0;
static GLuint mirrorProgram = 0;
static const float MIRROR_DISTANCE = 35.0f;  // Distance from center (moved to corners)
static const float MIRROR_WIDTH = 50.0f;
static const float MIRROR_HEIGHT = 70.0f;
static const float MIRROR_TILT = 15.0f;  // Tilt angle in degrees

// Framebuffer for mirror reflections (4 mirrors)
static const int MIRROR_TEX_SIZE = 512;  // Resolution of reflection textures
static GLuint mirrorFBO[4] = {0, 0, 0, 0};
static GLuint mirrorTexture[4] = {0, 0, 0, 0};
static GLuint mirrorDepthRBO[4] = {0, 0, 0, 0};

// Mirror positions at corners (4 mirrors only - no top/bottom)
// 0=front-left, 1=front-right, 2=back-left, 3=back-right
static bool mirrorBroken[4] = {false, false, false, false};
static float mirrorBreakTime[4] = {0.0f, 0.0f, 0.0f, 0.0f};

// World explosion state
static bool worldExploding = false;
static float worldExplosionTime = 0.0f;
static float worldExplosionDuration = 3.0f;

// Shatter particles for broken mirrors
struct ShatterParticle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec2 size;
    float rotation;
    float rotSpeed;
    float alpha;
    float lifetime;  // How long until it disappears
};
static std::vector<ShatterParticle> shatterParticles;
static GLuint shatterVAO = 0, shatterVBO = 0, shatterProgram = 0;

// Speedforce lightning
static float lightningFlash = 0.0f;
static float lightningTimer = 0.0f;
static float thunderTimer = 0.0f;

// Camera
static float cameraDistance = 15.0f;
static float cameraYaw = 0.0f;
static float cameraPitch = 20.0f;
static float lastMouseX = 640.0f;
static float lastMouseY = 360.0f;
static bool firstMouse = true;
static float mouseSensitivity = 0.2f;

// Input
static bool keyW = false, keyS = false, keyA = false, keyD = false;
static bool keySpace = false, keyShift = false;

// Animation 
static bool playAnimation = true;
static float playbackSpeed = 2.0f;
static float animationTime = 0.0f;

// Aura intensity (0 = none, 1 = full Super Saiyan)
static float auraIntensity = 0.7f;

// ---------------------------------------------------------------------------
// Bird Bots (flying in day world)
// ---------------------------------------------------------------------------
struct BirdBot {
    glm::vec3 position;      // World position (relative to character)
    glm::vec3 velocity;      // Flying direction and speed
    float wingPhase;         // Wing flap animation phase
    float wingSpeed;         // How fast wings flap
    float size;              // Bird size scale
};
static const int NUM_BIRDS = 8;
static std::vector<BirdBot> birdBots;
static GLuint birdBodyVAO = 0, birdBodyVBO = 0, birdBodyEBO = 0;
static GLuint birdWingVAO = 0, birdWingVBO = 0, birdWingEBO = 0;
static int birdBodyIndexCount = 0;
static int birdWingIndexCount = 0;

// ---------------------------------------------------------------------------
// Airplane (flies across day sky every 20 seconds)
// ---------------------------------------------------------------------------
struct Airplane {
    glm::vec3 position;
    glm::vec3 direction;
    float timer;             // Time since last crossing
    bool active;             // Currently visible
};
static Airplane airplane;
static GLuint airplaneBodyVAO = 0, airplaneBodyVBO = 0, airplaneBodyEBO = 0;
static GLuint airplaneWingVAO = 0, airplaneWingVBO = 0, airplaneWingEBO = 0;
static GLuint airplaneTailVAO = 0, airplaneTailVBO = 0, airplaneTailEBO = 0;
static int airplaneBodyIndexCount = 0;
static int airplaneWingIndexCount = 0;
static int airplaneTailIndexCount = 0;

// ---------------------------------------------------------------------------
// Ring Warriors Battle (night world) - Green vs Yellow
// ---------------------------------------------------------------------------
struct RingWarrior {
    glm::vec3 position;      // World position (relative to character)
    glm::vec3 velocity;      // Movement
    glm::vec3 homePosition;  // Original position to return to
    float bodyRotation;      // Y-axis rotation
    float armAngle;          // Arm raised angle for shooting
    bool isGreen;            // true = green hero, false = yellow villain
    float shootPhase;        // Animation phase for shooting
};
static RingWarrior greenHero;
static RingWarrior yellowVillain;

// Battle state: 0 = fighting/charging, 1 = exploding, 2 = returning to position
static int battleState = 0;
static float returnTimer = 0.0f;

// Energy beam collision
struct EnergyCollision {
    glm::vec3 position;      // Where beams meet
    float radius;            // Current explosion radius
    float timer;             // Time since collision started
    float maxTime;           // How long until explosion (10 seconds)
    bool exploding;          // In explosion phase
    float explosionTimer;    // Time during explosion
    float shockwaveRadius;   // Expanding shockwave
};
static EnergyCollision energyCollision;

// Beam particles
struct BeamParticle {
    glm::vec3 position;
    glm::vec3 velocity;
    float life;
    float size;
    bool isGreen;
};
static std::vector<BeamParticle> beamParticles;

// VAOs for ring warriors
static GLuint heroBodyVAO = 0, heroBodyVBO = 0, heroBodyEBO = 0;
static GLuint villainBodyVAO = 0, villainBodyVBO = 0, villainBodyEBO = 0;
static GLuint energyBallVAO = 0, energyBallVBO = 0, energyBallEBO = 0;
static GLuint greenBallVAO = 0, greenBallVBO = 0, greenBallEBO = 0;      // Green energy ball
static GLuint yellowBallVAO = 0, yellowBallVBO = 0, yellowBallEBO = 0;    // Yellow energy ball  
static GLuint mixedBallVAO = 0, mixedBallVBO = 0, mixedBallEBO = 0;      // Green-yellow mixed ball
static GLuint beamVAO = 0, beamVBO = 0;
// Extended arm VAOs for ring warriors (single arm pointing forward)
static GLuint heroArmVAO = 0, heroArmVBO = 0, heroArmEBO = 0;
static GLuint villainArmVAO = 0, villainArmVBO = 0, villainArmEBO = 0;
static int heroArmIndexCount = 0, villainArmIndexCount = 0;
static int heroBodyIndexCount = 0;
static int villainBodyIndexCount = 0;
static int energyBallIndexCount = 0;

// ---------------------------------------------------------------------------
// Flash Racing (speedforce world) - Flash vs Reverse Flash
// ---------------------------------------------------------------------------
struct SpeedsterRunner {
    glm::vec3 position;      // Position relative to character
    glm::vec3 velocity;      // Current velocity
    float bodyRotation;      // Y-axis rotation (facing direction)
    float runPhase;          // Animation phase for running
    float armSwing;          // Arm swing angle
    float legSwing;          // Leg swing angle
    bool isFlash;            // true = red Flash, false = yellow Reverse Flash
    float laneOffset;        // Z offset for lane position
    float speedBoost;        // Random speed variation
    int lap;                 // Current lap number
};
static SpeedsterRunner theFlash;
static SpeedsterRunner reverseFlash;

// Racing track parameters
static float raceTrackRadius = 60.0f;      // Radius of circular track
static float raceTrackWidth = 15.0f;       // Width of track
static float flashRaceTimer = 0.0f;        // Time in current race
static int flashLeader = 0;                // 0 = Flash leading, 1 = Reverse Flash leading

// Lightning trail particles
struct LightningTrail {
    glm::vec3 position;
    glm::vec3 velocity;
    float life;
    float size;
    bool isRed;              // true = red (Flash), false = yellow (Reverse Flash)
};
static std::vector<LightningTrail> lightningTrails;

// VAOs for speedsters
static GLuint flashBodyVAO = 0, flashBodyVBO = 0, flashBodyEBO = 0;
static GLuint reverseFlashBodyVAO = 0, reverseFlashBodyVBO = 0, reverseFlashBodyEBO = 0;
static GLuint flashLightningVAO = 0, flashLightningVBO = 0, flashLightningEBO = 0;
static int flashBodyIndexCount = 0;
static int reverseFlashBodyIndexCount = 0;
static int flashLightningIndexCount = 0;

// ---------------------------------------------------------------------------
// Goku vs Cell Teleport Punch Battle (chrome world)
// ---------------------------------------------------------------------------
struct DBZFighter {
    glm::vec3 position;      // Position relative to character
    glm::vec3 homePosition;  // Original position
    glm::vec3 velocity;      // Movement velocity
    glm::vec3 targetPos;     // Teleport target position
    float bodyRotation;      // Y-axis rotation
    float armAngle;          // Arm angle for punching
    float powerLevel;        // Current power charge (0-1)
    float auraIntensity;     // Aura glow intensity
    bool isGoku;             // true = Goku, false = Cell
    float chargePhase;       // Animation phase
    bool isPunching;         // Currently throwing punch
    bool isHit;              // Got hit by punch
    float hitRecoil;         // Recoil animation
    float teleportTimer;     // Time until next teleport
    float punchTimer;        // Punch animation timer
    int comboCount;          // Current combo hits
};
static DBZFighter goku;
static DBZFighter cell;

// Punch effect particles
struct PunchEffect {
    glm::vec3 position;      // Where the punch landed
    float life;              // Effect duration
    float size;              // Effect size
    float intensity;         // Brightness
    bool isGokuPunch;        // Who threw the punch
    int effectType;          // 0 = impact, 1 = shockwave, 2 = speed lines
};
static std::vector<PunchEffect> punchEffects;

// Teleport trail effect
struct TeleportTrail {
    glm::vec3 position;      // Trail position
    float life;              // Fade out
    float size;              // Size of afterimage
    bool isGoku;             // Whose trail
    float rotation;          // Body rotation at this point
};
static std::vector<TeleportTrail> teleportTrails;

// DBZ Battle state: 0 = teleporting/fighting, 1 = clash (both punch at same time), 2 = big impact, 3 = returning
static int dbzBattleState = 0;
static float dbzChargeTimer = 0.0f;
static float dbzReturnTimer = 0.0f;
static float dbzTeleportInterval = 0.15f;  // How fast they teleport
static float dbzNextTeleport = 0.0f;       // Time until next teleport
static int dbzPunchCount = 0;              // Total punches in this round
static float dbzClashTimer = 0.0f;         // Timer for clash state
static glm::vec3 dbzClashPosition;         // Where they clashed

// Aura particles for DBZ fighters
struct DBZAuraParticle {
    glm::vec3 position;
    glm::vec3 velocity;
    float life;
    float size;
    bool isGoku;  // true = golden/blue (Goku), false = green (Cell)
};
static std::vector<DBZAuraParticle> dbzAuraParticles;

// Impact particles (replaces beam particles)
struct ImpactParticle {
    glm::vec3 position;
    glm::vec3 velocity;
    float life;
    float size;
    bool isGoku;
};
static std::vector<ImpactParticle> impactParticles;

// VAOs for DBZ fighters
static GLuint gokuBodyVAO = 0, gokuBodyVBO = 0, gokuBodyEBO = 0;
static GLuint cellBodyVAO = 0, cellBodyVBO = 0, cellBodyEBO = 0;
static GLuint punchImpactVAO = 0, punchImpactVBO = 0, punchImpactEBO = 0;  // Punch impact effect
static GLuint gokuKamehaVAO = 0, gokuKamehaVBO = 0, gokuKamehaEBO = 0;   // Gold energy ball for Goku
static GLuint cellKamehaVAO = 0, cellKamehaVBO = 0, cellKamehaEBO = 0;   // Green energy ball for Cell
static GLuint clashBallVAO = 0, clashBallVBO = 0, clashBallEBO = 0;       // Clash energy ball
// Extended arm VAOs for DBZ fighters (punching arm)
static GLuint gokuArmsVAO = 0, gokuArmsVBO = 0, gokuArmsEBO = 0;
static GLuint cellArmsVAO = 0, cellArmsVBO = 0, cellArmsEBO = 0;
static int gokuArmsIndexCount = 0, cellArmsIndexCount = 0;
static int gokuBodyIndexCount = 0;
static int cellBodyIndexCount = 0;
static int punchImpactIndexCount = 0;
static int kamehaBeamIndexCount = 0;

// ---------------------------------------------------------------------------
// Aura Particle System (Super Saiyan energy effect)
// ---------------------------------------------------------------------------
struct AuraParticle {
    glm::vec3 Position;
    glm::vec3 Velocity;
    glm::vec4 Color;
    float Life;
    float Size;
};

class AuraSystem {
public:
    std::vector<AuraParticle> particles;
    int maxParticles;
    GLuint VAO, VBO;
    GLuint programID;
    GLuint mvpID, colorID;

    AuraSystem() { maxParticles = 500; }
    
    void initialize() {
        float vertices[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.5f,  0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Inline shaders for aura
        const char* vsSource = R"(
            #version 330 core
            layout(location = 0) in vec3 vertexPosition;
            uniform mat4 MVP;
            uniform vec4 particleColor;
            out vec4 Color;
            void main() {
                gl_Position = MVP * vec4(vertexPosition, 1.0);
                Color = particleColor;
            }
        )";
        
        const char* fsSource = R"(
            #version 330 core
            in vec4 Color;
            out vec4 FragColor;
            void main() {
                vec2 coord = gl_PointCoord;
                float dist = length(coord - vec2(0.5));
                float alpha = smoothstep(0.5, 0.0, dist) * Color.a;
                FragColor = vec4(Color.rgb, alpha);
            }
        )";
        
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vsSource, nullptr);
        glCompileShader(vs);
        
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fsSource, nullptr);
        glCompileShader(fs);
        
        programID = glCreateProgram();
        glAttachShader(programID, vs);
        glAttachShader(programID, fs);
        glLinkProgram(programID);
        
        glDeleteShader(vs);
        glDeleteShader(fs);
        
        mvpID = glGetUniformLocation(programID, "MVP");
        colorID = glGetUniformLocation(programID, "particleColor");
    }
    
    void update(float deltaTime, glm::vec3 charPos, float currentAura) {
        int newParticles = (int)(deltaTime * 500.0f * currentAura);
        if (currentAura > 0.1f && newParticles == 0) newParticles = 1;

        for (int i = 0; i < newParticles; i++) {
            if (particles.size() < (size_t)maxParticles) {
                AuraParticle p;
                float rX = ((rand() % 100) / 50.0f - 1.0f) * 1.5f;
                float rZ = ((rand() % 100) / 50.0f - 1.0f) * 1.5f;
                p.Position = charPos + glm::vec3(rX, 0.5f, rZ);
                p.Velocity = glm::vec3(rX * 0.5f, 3.0f + currentAura * 5.0f, rZ * 0.5f);
                // Color from orange/red to golden yellow
                p.Color = glm::mix(glm::vec4(1.0, 0.3, 0.0, 1.0), glm::vec4(1.0, 0.9, 0.3, 1.0), currentAura);
                p.Life = 1.0f;
                p.Size = 0.3f + currentAura * 0.5f;
                particles.push_back(p);
            }
        }

        for (size_t i = 0; i < particles.size(); ) {
            AuraParticle &p = particles[i];
            p.Life -= deltaTime * 1.5f;
            p.Position += p.Velocity * deltaTime;
            p.Color.a = p.Life * 0.8f;
            p.Size *= 1.0f + deltaTime * 0.5f;

            if (p.Life <= 0.0f) {
                particles.erase(particles.begin() + i);
            } else {
                i++;
            }
        }
    }
    
    void render(glm::mat4 view, glm::mat4 proj) {
        if (particles.empty()) return;
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        glUseProgram(programID);
        glBindVertexArray(VAO);

        for (const AuraParticle &p : particles) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, p.Position);
            
            // Billboarding
            model[0][0] = view[0][0]; model[0][1] = view[1][0]; model[0][2] = view[2][0];
            model[1][0] = view[0][1]; model[1][1] = view[1][1]; model[1][2] = view[2][1];
            model[2][0] = view[0][2]; model[2][1] = view[1][2]; model[2][2] = view[2][2];
            
            model = glm::scale(model, glm::vec3(p.Size));
            glm::mat4 mvp = proj * view * model;

            glUniformMatrix4fv(mvpID, 1, GL_FALSE, &mvp[0][0]);
            glUniform4fv(colorID, 1, &p.Color[0]);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    
    void cleanup() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteProgram(programID);
    }
};

static AuraSystem auraSystem;

// ---------------------------------------------------------------------------
// GLTF Bot Model (animated character with skinning)
// ---------------------------------------------------------------------------
struct MyBot {
    GLuint mvpMatrixID;
    GLuint jointMatricesID;
    GLuint lightPositionID;
    GLuint lightIntensityID;
    GLuint programID;
    GLuint baseColorID;
    GLuint viewPosID;
    GLuint worldTypeID;

    tinygltf::Model model;

    struct PrimitiveObject {
        GLuint vao;
        std::map<int, GLuint> vbos;
    };
    std::vector<PrimitiveObject> primitiveObjects;

    struct SkinObject {
        std::vector<glm::mat4> inverseBindMatrices;
        std::vector<glm::mat4> globalJointTransforms;
        std::vector<glm::mat4> jointMatrices;
    };
    std::vector<SkinObject> skinObjects;

    struct SamplerObject {
        std::vector<float> input;
        std::vector<glm::vec4> output;
        int interpolation;
    };
    struct AnimationObject {
        std::vector<SamplerObject> samplers;
    };
    std::vector<AnimationObject> animationObjects;

    glm::mat4 getNodeTransform(const tinygltf::Node& node) {
        glm::mat4 transform(1.0f);
        if (node.matrix.size() == 16) {
            transform = glm::make_mat4(node.matrix.data());
        } else {
            if (node.translation.size() == 3) {
                transform = glm::translate(transform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
            }
            if (node.rotation.size() == 4) {
                glm::quat q((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
                transform *= glm::mat4_cast(q);
            }
            if (node.scale.size() == 3) {
                transform = glm::scale(transform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
            }
        }
        return transform;
    }

    void computeLocalNodeTransform(const tinygltf::Model& mdl, int nodeIndex, std::vector<glm::mat4> &localTransforms) {
        const tinygltf::Node &node = mdl.nodes[nodeIndex];
        localTransforms[nodeIndex] = getNodeTransform(node);
        for (int childIndex : node.children) {
            computeLocalNodeTransform(mdl, childIndex, localTransforms);
        }
    }

    void computeGlobalNodeTransform(const tinygltf::Model& mdl, const std::vector<glm::mat4> &localTransforms,
                                    int nodeIndex, const glm::mat4& parentTransform, std::vector<glm::mat4> &globalTransforms) {
        glm::mat4 global = parentTransform * localTransforms[nodeIndex];
        globalTransforms[nodeIndex] = global;
        const tinygltf::Node &node = mdl.nodes[nodeIndex];
        for (int childIndex : node.children) {
            computeGlobalNodeTransform(mdl, localTransforms, childIndex, global, globalTransforms);
        }
    }

    std::vector<SkinObject> prepareSkinning(const tinygltf::Model &mdl) {
        std::vector<SkinObject> skins;
        for (size_t i = 0; i < mdl.skins.size(); i++) {
            SkinObject skinObject;
            const tinygltf::Skin &skin = mdl.skins[i];

            const tinygltf::Accessor &accessor = mdl.accessors[skin.inverseBindMatrices];
            const tinygltf::BufferView &bufferView = mdl.bufferViews[accessor.bufferView];
            const tinygltf::Buffer &buffer = mdl.buffers[bufferView.buffer];
            const float *ptr = reinterpret_cast<const float *>(buffer.data.data() + accessor.byteOffset + bufferView.byteOffset);

            skinObject.inverseBindMatrices.resize(accessor.count);
            for (size_t j = 0; j < accessor.count; j++) {
                float m[16];
                memcpy(m, ptr + j * 16, 16 * sizeof(float));
                skinObject.inverseBindMatrices[j] = glm::make_mat4(m);
            }

            skinObject.globalJointTransforms.resize(skin.joints.size());
            skinObject.jointMatrices.resize(skin.joints.size());

            std::vector<glm::mat4> localNodeTransforms(mdl.nodes.size(), glm::mat4(1.0f));
            std::vector<glm::mat4> globalNodeTransforms(mdl.nodes.size(), glm::mat4(1.0f));

            const tinygltf::Scene &scene = mdl.scenes[mdl.defaultScene];
            for (size_t r = 0; r < scene.nodes.size(); ++r) {
                computeLocalNodeTransform(mdl, scene.nodes[r], localNodeTransforms);
            }

            glm::mat4 identity(1.0f);
            for (size_t r = 0; r < scene.nodes.size(); ++r) {
                computeGlobalNodeTransform(mdl, localNodeTransforms, scene.nodes[r], identity, globalNodeTransforms);
            }

            for (size_t j = 0; j < skin.joints.size(); ++j) {
                int nodeIndex = skin.joints[j];
                skinObject.globalJointTransforms[j] = globalNodeTransforms[nodeIndex];
                skinObject.jointMatrices[j] = globalNodeTransforms[nodeIndex] * skinObject.inverseBindMatrices[j];
            }

            skins.push_back(skinObject);
        }
        return skins;
    }

    int findKeyframeIndex(const std::vector<float>& times, float animTime) {
        int left = 0, right = (int)times.size() - 1;
        while (left <= right) {
            int mid = (left + right) / 2;
            if (mid + 1 < (int)times.size() && times[mid] <= animTime && animTime < times[mid + 1]) {
                return mid;
            } else if (times[mid] > animTime) {
                right = mid - 1;
            } else {
                left = mid + 1;
            }
        }
        return (int)times.size() - 2;
    }

    std::vector<AnimationObject> prepareAnimation(const tinygltf::Model &mdl) {
        std::vector<AnimationObject> anims;
        for (const auto &anim : mdl.animations) {
            AnimationObject animationObject;
            for (const auto &sampler : anim.samplers) {
                SamplerObject samplerObject;

                const tinygltf::Accessor &inputAccessor = mdl.accessors[sampler.input];
                const tinygltf::BufferView &inputBufferView = mdl.bufferViews[inputAccessor.bufferView];
                const tinygltf::Buffer &inputBuffer = mdl.buffers[inputBufferView.buffer];

                samplerObject.input.resize(inputAccessor.count);
                const unsigned char *inputPtr = &inputBuffer.data[inputBufferView.byteOffset + inputAccessor.byteOffset];
                int stride = inputAccessor.ByteStride(inputBufferView);
                for (size_t j = 0; j < inputAccessor.count; ++j) {
                    samplerObject.input[j] = *reinterpret_cast<const float*>(inputPtr + j * stride);
                }

                const tinygltf::Accessor &outputAccessor = mdl.accessors[sampler.output];
                const tinygltf::BufferView &outputBufferView = mdl.bufferViews[outputAccessor.bufferView];
                const tinygltf::Buffer &outputBuffer = mdl.buffers[outputBufferView.buffer];

                const unsigned char *outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
                samplerObject.output.resize(outputAccessor.count);
                for (size_t j = 0; j < outputAccessor.count; ++j) {
                    if (outputAccessor.type == TINYGLTF_TYPE_VEC3) {
                        memcpy(&samplerObject.output[j], outputPtr + j * 3 * sizeof(float), 3 * sizeof(float));
                    } else if (outputAccessor.type == TINYGLTF_TYPE_VEC4) {
                        memcpy(&samplerObject.output[j], outputPtr + j * 4 * sizeof(float), 4 * sizeof(float));
                    }
                }

                animationObject.samplers.push_back(samplerObject);
            }
            anims.push_back(animationObject);
        }
        return anims;
    }

    void updateAnimation(const tinygltf::Model &mdl, const tinygltf::Animation &anim,
                         const AnimationObject &animationObject, float time, std::vector<glm::mat4> &nodeTransforms) {
        for (const auto &channel : anim.channels) {
            int targetNodeIndex = channel.target_node;
            const auto &sampler = anim.samplers[channel.sampler];

            const tinygltf::Accessor &outputAccessor = mdl.accessors[sampler.output];
            const tinygltf::BufferView &outputBufferView = mdl.bufferViews[outputAccessor.bufferView];
            const tinygltf::Buffer &outputBuffer = mdl.buffers[outputBufferView.buffer];

            const std::vector<float> &times = animationObject.samplers[channel.sampler].input;
            float animTime = fmod(time, times.back());

            int keyframeIndex = findKeyframeIndex(times, animTime);
            int nextIndex = keyframeIndex + 1;
            if (nextIndex >= (int)times.size()) nextIndex = keyframeIndex;

            float t0 = times[keyframeIndex];
            float t1 = times[nextIndex];
            float factor = (t1 > t0) ? glm::clamp((animTime - t0) / (t1 - t0), 0.0f, 1.0f) : 0.0f;

            const unsigned char *outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];

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

    void updateSkinning(const std::vector<glm::mat4> &nodeTransforms) {
        if (model.skins.empty() || skinObjects.empty()) return;
        for (size_t s = 0; s < model.skins.size(); ++s) {
            const tinygltf::Skin &skin = model.skins[s];
            SkinObject &skinObject = skinObjects[s];
            for (size_t j = 0; j < skin.joints.size(); ++j) {
                int nodeIndex = skin.joints[j];
                skinObject.globalJointTransforms[j] = nodeTransforms[nodeIndex];
                skinObject.jointMatrices[j] = nodeTransforms[nodeIndex] * skinObject.inverseBindMatrices[j];
            }
        }
    }

    // Movement-based animation: gliding forward/sideways, floating on back when backward
    // moveDir: 0=idle/tumble, 1=forward, 2=left, 3=right, 4=backward (floating on back)
    void updateMovementPose(float time, int moveDir, float moveBlend) {
        if (model.skins.empty() || skinObjects.empty()) return;
        
        // Get base transforms from T-pose
        std::vector<glm::mat4> localNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
        for (size_t i = 0; i < model.nodes.size(); ++i) {
            localNodeTransforms[i] = getNodeTransform(model.nodes[i]);
        }
        
        // Find body nodes by name
        int leftShoulderIdx = -1, rightShoulderIdx = -1;
        int leftForeArmIdx = -1, rightForeArmIdx = -1;
        int leftHandIdx = -1, rightHandIdx = -1;
        int leftUpLegIdx = -1, rightUpLegIdx = -1;
        int leftLegIdx = -1, rightLegIdx = -1;
        int spineIdx = -1, spine1Idx = -1, spine2Idx = -1;
        int headIdx = -1, neckIdx = -1;
        
        for (size_t i = 0; i < model.nodes.size(); ++i) {
            const std::string& name = model.nodes[i].name;
            if (name == "LeftShoulder") leftShoulderIdx = (int)i;
            else if (name == "RightShoulder") rightShoulderIdx = (int)i;
            else if (name == "LeftForeArm") leftForeArmIdx = (int)i;
            else if (name == "RightForeArm") rightForeArmIdx = (int)i;
            else if (name == "LeftHand") leftHandIdx = (int)i;
            else if (name == "RightHand") rightHandIdx = (int)i;
            else if (name == "LeftUpLeg") leftUpLegIdx = (int)i;
            else if (name == "RightUpLeg") rightUpLegIdx = (int)i;
            else if (name == "LeftLeg") leftLegIdx = (int)i;
            else if (name == "RightLeg") rightLegIdx = (int)i;
            else if (name == "Spine") spineIdx = (int)i;
            else if (name == "Spine1") spine1Idx = (int)i;
            else if (name == "Spine2") spine2Idx = (int)i;
            else if (name == "Head") headIdx = (int)i;
            else if (name == "Neck") neckIdx = (int)i;
        }
        
        // Subtle wind flutter animation
        float flutter = sin(time * 3.0f) * 0.05f;
        float flutter2 = cos(time * 2.5f) * 0.03f;
        
        if (moveDir == 1 || moveDir == 5 || moveDir == 6) {
            // DIVING POSE for north - arms tight against torso
            float blend = moveBlend;
            
            // Arms touching torso - reversed direction (outward instead of inward)
            if (leftShoulderIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-70.0f) * blend, glm::vec3(0, 0, 1));  // Arms outward
                rot = glm::rotate(rot, glm::radians(10.0f + flutter * 3.0f) * blend, glm::vec3(1, 0, 0));  // Slightly forward
                localNodeTransforms[leftShoulderIdx] = localNodeTransforms[leftShoulderIdx] * rot;
            }
            if (rightShoulderIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(70.0f) * blend, glm::vec3(0, 0, 1));  // Arms outward
                rot = glm::rotate(rot, glm::radians(10.0f + flutter2 * 3.0f) * blend, glm::vec3(1, 0, 0));  // Slightly forward
                localNodeTransforms[rightShoulderIdx] = localNodeTransforms[rightShoulderIdx] * rot;
            }
            
            // Forearms bent opposite direction
            if (leftForeArmIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-5.0f + flutter * 2.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[leftForeArmIdx] = localNodeTransforms[leftForeArmIdx] * rot;
            }
            if (rightForeArmIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-5.0f + flutter2 * 2.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[rightForeArmIdx] = localNodeTransforms[rightForeArmIdx] * rot;
            }
            
            // Hands clenched into fists (curl fingers opposite direction)
            if (leftHandIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f) * blend, glm::vec3(1, 0, 0));  // Curl hand into fist
                localNodeTransforms[leftHandIdx] = localNodeTransforms[leftHandIdx] * rot;
            }
            if (rightHandIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f) * blend, glm::vec3(1, 0, 0));  // Curl hand into fist
                localNodeTransforms[rightHandIdx] = localNodeTransforms[rightHandIdx] * rot;
            }
            
            // Legs slightly apart and relaxed
            if (leftUpLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-10.0f) * blend, glm::vec3(0, 0, 1));  // Legs apart
                rot = glm::rotate(rot, glm::radians(10.0f + flutter * 3.0f) * blend, glm::vec3(1, 0, 0));  // Slight bend
                localNodeTransforms[leftUpLegIdx] = localNodeTransforms[leftUpLegIdx] * rot;
            }
            if (rightUpLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(10.0f) * blend, glm::vec3(0, 0, 1));
                rot = glm::rotate(rot, glm::radians(10.0f + flutter2 * 3.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[rightUpLegIdx] = localNodeTransforms[rightUpLegIdx] * rot;
            }
            
            // Knees slightly bent
            if (leftLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(15.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[leftLegIdx] = localNodeTransforms[leftLegIdx] * rot;
            }
            if (rightLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(15.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[rightLegIdx] = localNodeTransforms[rightLegIdx] * rot;
            }
            
            // Head tilted back slightly
            if (headIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-15.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[headIdx] = localNodeTransforms[headIdx] * rot;
            }
            
        } else if (moveDir == 4) {
            // SUPERMAN POSE - arms stretched forward along Y when moving south/backward
            float blend = moveBlend;
            
            // Arms stretched FORWARD along Y axis (parallel with face)
            if (leftShoulderIdx >= 0) {
                // Rotate around Y axis to bring arms forward
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f + flutter * 3.0f) * blend, glm::vec3(0, 1, 0));  // Arms forward along Y
                localNodeTransforms[leftShoulderIdx] = localNodeTransforms[leftShoulderIdx] * rot;
            }
            if (rightShoulderIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f + flutter2 * 3.0f) * blend, glm::vec3(0, 1, 0));  // Arms forward along Y
                localNodeTransforms[rightShoulderIdx] = localNodeTransforms[rightShoulderIdx] * rot;
            }
            
            // Forearms straight - fully extended
            if (leftForeArmIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[leftForeArmIdx] = localNodeTransforms[leftForeArmIdx] * rot;
            }
            if (rightForeArmIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[rightForeArmIdx] = localNodeTransforms[rightForeArmIdx] * rot;
            }
            
            // Legs together and straight back
            if (leftUpLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(5.0f) * blend, glm::vec3(0, 0, 1));  // Legs together
                rot = glm::rotate(rot, glm::radians(-20.0f + flutter * 3.0f) * blend, glm::vec3(1, 0, 0));  // Back
                localNodeTransforms[leftUpLegIdx] = localNodeTransforms[leftUpLegIdx] * rot;
            }
            if (rightUpLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-5.0f) * blend, glm::vec3(0, 0, 1));
                rot = glm::rotate(rot, glm::radians(-20.0f + flutter2 * 3.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[rightUpLegIdx] = localNodeTransforms[rightUpLegIdx] * rot;
            }
            
            // Slight knee bend
            if (leftLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(20.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[leftLegIdx] = localNodeTransforms[leftLegIdx] * rot;
            }
            if (rightLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(20.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[rightLegIdx] = localNodeTransforms[rightLegIdx] * rot;
            }
            
            // Head looking forward/down
            if (headIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(25.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[headIdx] = localNodeTransforms[headIdx] * rot;
            }
            
            // Slight spine arch for heroic pose
            if (spineIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-10.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[spineIdx] = localNodeTransforms[spineIdx] * rot;
            }
        } else if (moveDir == 2 || moveDir == 3) {
            // GLIDING POSE - arms back like wings when moving pure left/right
            float blend = moveBlend;
            
            // Arms stretched back like wings
            if (leftShoulderIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-60.0f + flutter * 5.0f) * blend, glm::vec3(0, 0, 1));  // Arms back
                rot = glm::rotate(rot, glm::radians(-30.0f) * blend, glm::vec3(1, 0, 0));  // Angled back
                localNodeTransforms[leftShoulderIdx] = localNodeTransforms[leftShoulderIdx] * rot;
            }
            if (rightShoulderIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(60.0f + flutter2 * 5.0f) * blend, glm::vec3(0, 0, 1));
                rot = glm::rotate(rot, glm::radians(-30.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[rightShoulderIdx] = localNodeTransforms[rightShoulderIdx] * rot;
            }
            
            // Forearms extended
            if (leftForeArmIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(10.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[leftForeArmIdx] = localNodeTransforms[leftForeArmIdx] * rot;
            }
            if (rightForeArmIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(10.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[rightForeArmIdx] = localNodeTransforms[rightForeArmIdx] * rot;
            }
            
            // Legs together and straight back
            if (leftUpLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(5.0f) * blend, glm::vec3(0, 0, 1));  // Legs together
                rot = glm::rotate(rot, glm::radians(-20.0f + flutter * 3.0f) * blend, glm::vec3(1, 0, 0));  // Back
                localNodeTransforms[leftUpLegIdx] = localNodeTransforms[leftUpLegIdx] * rot;
            }
            if (rightUpLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-5.0f) * blend, glm::vec3(0, 0, 1));
                rot = glm::rotate(rot, glm::radians(-20.0f + flutter2 * 3.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[rightUpLegIdx] = localNodeTransforms[rightUpLegIdx] * rot;
            }
            
            // Slight knee bend
            if (leftLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(20.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[leftLegIdx] = localNodeTransforms[leftLegIdx] * rot;
            }
            if (rightLegIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(20.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[rightLegIdx] = localNodeTransforms[rightLegIdx] * rot;
            }
            
            // Head looking forward
            if (headIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(20.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[headIdx] = localNodeTransforms[headIdx] * rot;
            }
            
            // Slight spine arch for heroic pose
            if (spineIdx >= 0) {
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(-10.0f) * blend, glm::vec3(1, 0, 0));
                localNodeTransforms[spineIdx] = localNodeTransforms[spineIdx] * rot;
            }
        }
        // moveDir == 0: idle/tumble - keep T-pose (no modifications)
        
        // Compute global transforms
        std::vector<glm::mat4> globalNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
        const tinygltf::Scene &scene = model.scenes[model.defaultScene];
        glm::mat4 identity(1.0f);
        for (size_t r = 0; r < scene.nodes.size(); ++r) {
            computeGlobalNodeTransform(model, localNodeTransforms, scene.nodes[r], identity, globalNodeTransforms);
        }
        
        // Update skinning matrices
        updateSkinning(globalNodeTransforms);
    }

    void update(float time) {
        // Default: no movement, use T-pose
        updateMovementPose(time, 0, 0.0f);
    }

    bool loadModel(tinygltf::Model &mdl, const char *filename) {
        tinygltf::TinyGLTF loader;
        std::string err, warn;
        bool res = loader.LoadASCIIFromFile(&mdl, &err, &warn, filename);
        if (!warn.empty()) std::cout << "WARN: " << warn << std::endl;
        if (!err.empty()) std::cout << "ERR: " << err << std::endl;
        if (!res) std::cout << "Failed to load glTF: " << filename << std::endl;
        else std::cout << "Loaded glTF: " << filename << std::endl;
        return res;
    }

    void initialize() {
        if (!loadModel(model, "../project/model/bot/bot.gltf")) {
            return;
        }
        primitiveObjects = bindModel(model);
        skinObjects = prepareSkinning(model);
        animationObjects = prepareAnimation(model);

        // Enhanced shader with PBR-style lighting
        const char* vsSource = R"(
            #version 330 core
            layout(location = 0) in vec3 vertexPosition;
            layout(location = 1) in vec3 vertexNormal;
            layout(location = 2) in vec2 vertexUV;
            layout(location = 3) in vec4 vertexJoints;
            layout(location = 4) in vec4 vertexWeights;
            
            out vec3 worldPosition;
            out vec3 worldNormal;
            out vec2 fragUV;
            
            uniform mat4 MVP;
            uniform mat4 modelMatrix;
            uniform mat4 jointMatrices[64];
            
            void main() {
                int j0 = int(vertexJoints.x);
                int j1 = int(vertexJoints.y);
                int j2 = int(vertexJoints.z);
                int j3 = int(vertexJoints.w);
                
                mat4 skinMatrix = vertexWeights.x * jointMatrices[j0]
                                + vertexWeights.y * jointMatrices[j1]
                                + vertexWeights.z * jointMatrices[j2]
                                + vertexWeights.w * jointMatrices[j3];
                
                vec4 skinnedPos = skinMatrix * vec4(vertexPosition, 1.0);
                vec3 skinnedNormal = mat3(skinMatrix) * vertexNormal;
                
                vec4 worldPos = modelMatrix * skinnedPos;
                gl_Position = MVP * skinnedPos;
                worldPosition = worldPos.xyz;
                worldNormal = normalize(mat3(modelMatrix) * skinnedNormal);
                fragUV = vertexUV;
            }
        )";

        const char* fsSource = R"(
            #version 330 core
            in vec3 worldPosition;
            in vec3 worldNormal;
            in vec2 fragUV;
            
            out vec4 finalColor;
            
            uniform vec3 lightPosition;
            uniform vec3 lightIntensity;
            uniform vec3 viewPos;
            uniform vec3 baseColor;
            uniform int worldType;
            
            void main() {
                // Material colors based on UV/position for variety
                vec3 jointColor = vec3(0.15, 0.21, 0.22);   // Dark metallic joints
                vec3 bodyColor = vec3(0.1, 0.42, 0.52);      // Teal body
                
                // Use position to determine body vs joints
                float bodyFactor = smoothstep(-0.3, 0.3, worldNormal.y);
                vec3 matColor = mix(jointColor, bodyColor, bodyFactor);
                matColor = mix(matColor, baseColor, 0.3);  // Blend with uniform base
                
                // Lighting calculation
                vec3 N = normalize(worldNormal);
                vec3 L = normalize(lightPosition - worldPosition);
                vec3 V = normalize(viewPos - worldPosition);
                vec3 H = normalize(L + V);
                
                // Ambient
                vec3 ambient = 0.15 * matColor;
                
                // World-specific ambient adjustment
                if (worldType == 1) {
                    // Space - darker ambient with slight blue tint
                    ambient = 0.08 * matColor + vec3(0.02, 0.03, 0.05);
                } else if (worldType == 2) {
                    // Chrome - reflective ambient
                    ambient = 0.25 * matColor + vec3(0.1, 0.1, 0.15);
                } else if (worldType == 3) {
                    // Speedforce - warm energy glow
                    ambient = 0.2 * matColor + vec3(0.1, 0.05, 0.0);
                } else if (worldType == 4) {
                    // Hall of Mirrors - golden warm light
                    ambient = 0.2 * matColor + vec3(0.08, 0.06, 0.02);
                }
                
                // Diffuse (half-lambert for softer shadows)
                float NdotL = dot(N, L);
                float diffuseFactor = NdotL * 0.5 + 0.5;
                diffuseFactor = diffuseFactor * diffuseFactor;
                vec3 diffuse = diffuseFactor * matColor * lightIntensity * 0.0000001;
                
                // Specular (Blinn-Phong with metallic look)
                float NdotH = max(dot(N, H), 0.0);
                float spec = pow(NdotH, 64.0);
                vec3 specular = vec3(0.5) * spec;
                
                // Fresnel rim lighting
                float fresnel = 1.0 - max(dot(N, V), 0.0);
                fresnel = pow(fresnel, 3.0);
                vec3 rimColor = vec3(0.4, 0.6, 0.8);
                
                // World-specific rim colors
                if (worldType == 2) rimColor = vec3(0.8, 0.8, 0.9);  // Chrome
                else if (worldType == 3) rimColor = vec3(1.0, 0.6, 0.2);  // Speedforce orange
                else if (worldType == 4) rimColor = vec3(0.9, 0.8, 0.5);  // Golden
                
                vec3 rim = rimColor * fresnel * 0.5;
                
                // Combine
                vec3 color = ambient + diffuse + specular + rim;
                
                // Tone mapping
                color = color / (1.0 + color);
                
                // Gamma correction
                color = pow(color, vec3(1.0 / 2.2));
                
                finalColor = vec4(color, 1.0);
            }
        )";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vsSource, nullptr);
        glCompileShader(vs);
        
        // Check for compile errors
        GLint success;
        glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(vs, 512, NULL, infoLog);
            std::cerr << "Bot vertex shader error: " << infoLog << std::endl;
        }

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fsSource, nullptr);
        glCompileShader(fs);
        
        glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(fs, 512, NULL, infoLog);
            std::cerr << "Bot fragment shader error: " << infoLog << std::endl;
        }

        programID = glCreateProgram();
        glAttachShader(programID, vs);
        glAttachShader(programID, fs);
        glLinkProgram(programID);

        glDeleteShader(vs);
        glDeleteShader(fs);

        mvpMatrixID = glGetUniformLocation(programID, "MVP");
        lightPositionID = glGetUniformLocation(programID, "lightPosition");
        lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
        jointMatricesID = glGetUniformLocation(programID, "jointMatrices");
        baseColorID = glGetUniformLocation(programID, "baseColor");
        viewPosID = glGetUniformLocation(programID, "viewPos");
        worldTypeID = glGetUniformLocation(programID, "worldType");
    }

    void bindMesh(std::vector<PrimitiveObject> &prims, tinygltf::Model &mdl, tinygltf::Mesh &mesh) {
        std::map<int, GLuint> vbos;
        for (size_t i = 0; i < mdl.bufferViews.size(); ++i) {
            const tinygltf::BufferView &bufferView = mdl.bufferViews[i];
            if (bufferView.target == 0) continue;
            const tinygltf::Buffer &buffer = mdl.buffers[bufferView.buffer];
            GLuint vbo;
            glGenBuffers(1, &vbo);
            glBindBuffer(bufferView.target, vbo);
            glBufferData(bufferView.target, bufferView.byteLength, &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
            vbos[i] = vbo;
        }

        for (size_t i = 0; i < mesh.primitives.size(); ++i) {
            tinygltf::Primitive primitive = mesh.primitives[i];
            GLuint vao;
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);

            for (auto &attrib : primitive.attributes) {
                tinygltf::Accessor accessor = mdl.accessors[attrib.second];
                int byteStride = accessor.ByteStride(mdl.bufferViews[accessor.bufferView]);
                glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);

                int size = (accessor.type != TINYGLTF_TYPE_SCALAR) ? accessor.type : 1;
                int vaa = -1;
                if (attrib.first == "POSITION") vaa = 0;
                if (attrib.first == "NORMAL") vaa = 1;
                if (attrib.first == "TEXCOORD_0") vaa = 2;
                if (attrib.first == "JOINTS_0") vaa = 3;
                if (attrib.first == "WEIGHTS_0") vaa = 4;

                if (vaa > -1) {
                    glEnableVertexAttribArray(vaa);
                    glVertexAttribPointer(vaa, size, accessor.componentType, accessor.normalized ? GL_TRUE : GL_FALSE, byteStride, BUFFER_OFFSET(accessor.byteOffset));
                }
            }

            PrimitiveObject primitiveObject;
            primitiveObject.vao = vao;
            primitiveObject.vbos = vbos;
            prims.push_back(primitiveObject);
            glBindVertexArray(0);
        }
    }

    void bindModelNodes(std::vector<PrimitiveObject> &prims, tinygltf::Model &mdl, tinygltf::Node &node) {
        if (node.mesh >= 0 && node.mesh < (int)mdl.meshes.size()) {
            bindMesh(prims, mdl, mdl.meshes[node.mesh]);
        }
        for (size_t i = 0; i < node.children.size(); i++) {
            bindModelNodes(prims, mdl, mdl.nodes[node.children[i]]);
        }
    }

    std::vector<PrimitiveObject> bindModel(tinygltf::Model &mdl) {
        std::vector<PrimitiveObject> prims;
        const tinygltf::Scene &scene = mdl.scenes[mdl.defaultScene];
        for (size_t i = 0; i < scene.nodes.size(); ++i) {
            bindModelNodes(prims, mdl, mdl.nodes[scene.nodes[i]]);
        }
        return prims;
    }

    void drawMesh(const std::vector<PrimitiveObject> &prims, tinygltf::Model &mdl, tinygltf::Mesh &mesh) {
        for (size_t i = 0; i < mesh.primitives.size(); ++i) {
            GLuint vao = prims[i].vao;
            glBindVertexArray(vao);
            tinygltf::Primitive primitive = mesh.primitives[i];
            tinygltf::Accessor indexAccessor = mdl.accessors[primitive.indices];
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prims[i].vbos.at(indexAccessor.bufferView));
            glDrawElements(primitive.mode, indexAccessor.count, indexAccessor.componentType, BUFFER_OFFSET(indexAccessor.byteOffset));
            glBindVertexArray(0);
        }
    }

    void drawModelNodes(const std::vector<PrimitiveObject>& prims, tinygltf::Model &mdl, tinygltf::Node &node) {
        if (node.mesh >= 0 && node.mesh < (int)mdl.meshes.size()) {
            drawMesh(prims, mdl, mdl.meshes[node.mesh]);
        }
        for (size_t i = 0; i < node.children.size(); i++) {
            drawModelNodes(prims, mdl, mdl.nodes[node.children[i]]);
        }
    }

    void drawModel(const std::vector<PrimitiveObject>& prims, tinygltf::Model &mdl) {
        const tinygltf::Scene &scene = mdl.scenes[mdl.defaultScene];
        for (size_t i = 0; i < scene.nodes.size(); ++i) {
            drawModelNodes(prims, mdl, mdl.nodes[scene.nodes[i]]);
        }
    }

    void render(glm::mat4 mvp, glm::mat4 modelMat, glm::vec3 viewPosition, glm::vec3 lightPos, glm::vec3 lightInt, int worldType) {
        glUseProgram(programID);
        glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(programID, "modelMatrix"), 1, GL_FALSE, &modelMat[0][0]);

        if (!skinObjects.empty() && jointMatricesID != -1) {
            const SkinObject &skinObject = skinObjects[0];
            if (!skinObject.jointMatrices.empty()) {
                glUniformMatrix4fv(jointMatricesID, (GLsizei)skinObject.jointMatrices.size(), GL_FALSE, glm::value_ptr(skinObject.jointMatrices[0]));
            }
        }

        glUniform3fv(lightPositionID, 1, &lightPos[0]);
        glUniform3fv(lightIntensityID, 1, &lightInt[0]);
        glUniform3fv(viewPosID, 1, &viewPosition[0]);
        glUniform3f(baseColorID, 0.2f, 0.5f, 0.7f);  // Base teal color
        glUniform1i(worldTypeID, worldType);

        drawModel(primitiveObjects, model);
    }

    void cleanup() {
        glDeleteProgram(programID);
    }
};

static MyBot bot;
static bool useGLTFBot = true;  // Toggle between GLTF and simple box character

// ---------------------------------------------------------------------------
// Skybox
// ---------------------------------------------------------------------------
static GLuint skyboxVAO = 0, skyboxVBO = 0;
static GLuint skyboxProgram = 0;

static void InitSkybox() {
    // Cube vertices for skybox (rendered inside-out)
    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // Compile skybox shader inline
    const char* vsSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        out vec3 vDir;
        uniform mat4 uVP;
        void main() {
            vDir = aPos;
            vec4 pos = uVP * vec4(aPos, 1.0);
            gl_Position = pos.xyww;
        }
    )";

    const char* fsSource = R"(
        #version 330 core
        in vec3 vDir;
        out vec4 FragColor;
        uniform float uTime;
        uniform float uFallDistance;
        uniform int uWorldType;  // 0 = day, 1 = night
        uniform float uTransition;
        
        // Hash function
        float hash(vec3 p) {
            p = fract(p * vec3(443.8975, 397.2973, 491.1871));
            p += dot(p, p.yxz + 19.19);
            return fract((p.x + p.y) * p.z);
        }
        
        float hash2(vec2 p) {
            return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
        }
        
        // 3D noise
        float noise(vec3 p) {
            vec3 i = floor(p);
            vec3 f = fract(p);
            f = f * f * (3.0 - 2.0 * f);
            
            return mix(
                mix(mix(hash(i), hash(i + vec3(1,0,0)), f.x),
                    mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
                mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
                    mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y),
                f.z
            );
        }
        
        // Fractal Brownian Motion for clouds
        float fbm(vec3 p) {
            float value = 0.0;
            float amplitude = 0.5;
            for (int i = 0; i < 5; i++) {
                value += amplitude * noise(p);
                p *= 2.0;
                amplitude *= 0.5;
            }
            return value;
        }
        
        // Stars for night sky
        float stars(vec3 dir) {
            vec3 p = dir * 300.0;
            vec3 i = floor(p);
            vec3 f = fract(p);
            
            float star = 0.0;
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    for (int z = -1; z <= 1; z++) {
                        vec3 cell = i + vec3(x, y, z);
                        vec3 starPos = cell + vec3(hash(cell), hash(cell + 100.0), hash(cell + 200.0)) * 0.8 + 0.1;
                        float d = length(p - starPos);
                        float brightness = hash(cell + 300.0);
                        if (brightness > 0.85) {
                            float twinkle = sin(uTime * (2.0 + brightness * 5.0) + brightness * 100.0) * 0.3 + 0.7;
                            star += smoothstep(0.15, 0.0, d) * twinkle * (brightness - 0.85) * 6.0;
                        }
                    }
                }
            }
            return star;
        }
        
        // Draw a single bird silhouette
        float drawBird(vec2 uv, vec2 pos, float size, float wingPhase) {
            vec2 p = uv - pos;
            p.x *= 1.5;  // Stretch horizontally
            
            // Wing flapping animation
            float wing = sin(wingPhase) * 0.3;
            
            // Body (small ellipse)
            float body = smoothstep(size * 0.15, 0.0, length(p * vec2(1.0, 2.0)));
            
            // Left wing
            vec2 lwing = p - vec2(-size * 0.2, wing * size);
            float leftWing = smoothstep(size * 0.1, 0.0, length(lwing * vec2(0.5, 2.0 - wing)));
            
            // Right wing
            vec2 rwing = p - vec2(size * 0.2, wing * size);
            float rightWing = smoothstep(size * 0.1, 0.0, length(rwing * vec2(0.5, 2.0 - wing)));
            
            return max(body, max(leftWing, rightWing));
        }
        
        // Draw a flock of birds
        vec3 drawBirds(vec3 dir, vec3 skyColor, float time) {
            // Only draw birds in upper sky
            if (dir.y < 0.1) return skyColor;
            
            vec3 result = skyColor;
            
            // Multiple bird flocks at different positions
            for (int flock = 0; flock < 3; flock++) {
                float flockOffset = float(flock) * 2.5;
                
                // Flock movement - circular path across sky
                float flockTime = time * 0.1 + flockOffset;
                vec3 flockCenter = vec3(
                    sin(flockTime) * 0.5,
                    0.3 + cos(flockTime * 0.7) * 0.15 + float(flock) * 0.1,
                    cos(flockTime) * 0.5
                );
                
                // Check if this direction is near the flock
                float flockDist = length(dir - normalize(flockCenter));
                if (flockDist < 0.3) {
                    // Draw individual birds in the flock
                    for (int b = 0; b < 5; b++) {
                        float birdId = float(flock * 5 + b);
                        vec2 offset = vec2(
                            sin(birdId * 1.7) * 0.05,
                            cos(birdId * 2.3) * 0.03
                        );
                        
                        vec3 birdDir = normalize(flockCenter + vec3(offset.x, offset.y, offset.x * 0.5));
                        float dist = length(dir - birdDir);
                        
                        // Wing flap phase varies per bird
                        float wingPhase = time * 8.0 + birdId * 1.5;
                        
                        if (dist < 0.02) {
                            float bird = smoothstep(0.02, 0.005, dist);
                            // Darker silhouette
                            result = mix(result, vec3(0.1, 0.1, 0.15), bird * 0.8);
                        }
                    }
                }
            }
            
            return result;
        }
        
        // Draw airplane
        vec3 drawAirplane(vec3 dir, vec3 skyColor, float time) {
            // Airplane flies across every 20 seconds
            float cycleTime = mod(time, 20.0);
            
            // Only visible for part of the cycle (flying across takes ~8 seconds)
            if (cycleTime > 10.0) return skyColor;
            
            // Airplane path - flies from left to right across upper sky
            float t = cycleTime / 10.0;  // 0 to 1 over 10 seconds
            vec3 planePos = vec3(
                mix(-0.8, 0.8, t),           // X: left to right
                0.25 + sin(t * 3.14159) * 0.05,  // Y: slight arc
                0.6                            // Z: in front
            );
            planePos = normalize(planePos);
            
            float planeDist = length(dir - planePos);
            
            if (planeDist < 0.05) {
                vec3 result = skyColor;
                
                // Airplane body
                float body = smoothstep(0.012, 0.005, planeDist);
                
                // Wings (check horizontal offset)
                vec3 toPlane = dir - planePos;
                float wingDist = abs(toPlane.y) + abs(toPlane.x - planePos.x) * 0.5;
                float wings = smoothstep(0.025, 0.015, planeDist) * smoothstep(0.01, 0.005, abs(toPlane.y));
                
                // Tail
                float tail = smoothstep(0.02, 0.01, planeDist + toPlane.x * 5.0) * smoothstep(0.008, 0.003, abs(toPlane.y - 0.005));
                
                // Combine airplane parts - white/silver color
                float plane = max(body, max(wings * 0.7, tail * 0.5));
                result = mix(result, vec3(0.9, 0.9, 0.95), plane);
                
                // Contrail behind airplane
                if (toPlane.x < 0.0 && abs(toPlane.y) < 0.003) {
                    float contrail = smoothstep(0.0, -0.15, toPlane.x) * smoothstep(0.003, 0.0, abs(toPlane.y));
                    contrail *= 1.0 - t * 0.5;  // Fade contrail over time
                    result = mix(result, vec3(1.0, 1.0, 1.0), contrail * 0.6);
                }
                
                return result;
            }
            
            return skyColor;
        }
        
        vec3 dayWorld(vec3 dir) {
            float y = dir.y;
            float cloudScroll = uFallDistance * 0.001;
            
            vec3 skyTop = vec3(0.1, 0.2, 0.5);
            vec3 skyMid = vec3(0.4, 0.6, 0.9);
            vec3 horizon = vec3(0.9, 0.7, 0.5);
            vec3 skyBottom = vec3(0.2, 0.3, 0.6);
            
            vec3 skyColor;
            if (y > 0.0) {
                float t = pow(y, 0.5);
                skyColor = mix(mix(horizon, skyMid, smoothstep(0.0, 0.3, y)), skyTop, t);
            } else {
                skyColor = mix(horizon, skyBottom, smoothstep(0.0, -0.5, y));
            }
            
            vec3 cloudPos = dir * 3.0 + vec3(uTime * 0.02, cloudScroll, uTime * 0.01);
            float clouds = fbm(cloudPos * 2.0);
            clouds = smoothstep(0.4, 0.8, clouds);
            
            float horizonFactor = 1.0 - abs(y);
            clouds *= horizonFactor * horizonFactor;
            
            vec3 cloudColor = vec3(1.0, 0.98, 0.95);
            skyColor = mix(skyColor, cloudColor, clouds * 0.7);
            
            float streaks = noise(vec3(dir.x * 10.0, dir.y * 2.0 + cloudScroll * 5.0, dir.z * 10.0));
            streaks = smoothstep(0.6, 0.9, streaks) * horizonFactor;
            skyColor = mix(skyColor, cloudColor, streaks * 0.3);
            
            vec3 sunDir = normalize(vec3(0.5, 0.3, 0.8));
            float sunDot = max(dot(dir, sunDir), 0.0);
            vec3 sunColor = vec3(1.0, 0.9, 0.7);
            skyColor += sunColor * pow(sunDot, 32.0) * 0.8;
            skyColor += sunColor * pow(sunDot, 4.0) * 0.3;
            
            float scatter = pow(horizonFactor, 3.0);
            skyColor = mix(skyColor, vec3(0.8, 0.85, 0.95), scatter * 0.2);
            
            // Birds and airplane are now 3D models, not shader effects
            
            return skyColor;
        }
        
        // SPACE VERSE - Solar system, galaxies, and stars
        vec3 spaceWorld(vec3 dir) {
            float y = dir.y;
            float cloudScroll = uFallDistance * 0.0005;
            
            // Deep space background - very dark with subtle color variations
            vec3 spaceDeep = vec3(0.0, 0.0, 0.02);     // Nearly black
            vec3 spaceMid = vec3(0.02, 0.01, 0.05);    // Deep purple hint
            vec3 spaceNebula = vec3(0.05, 0.02, 0.08); // Nebula purple
            
            vec3 skyColor = mix(spaceDeep, spaceMid, abs(y) * 0.5);
            
            // Dense star field - multiple layers
            float starField1 = stars(dir);
            float starField2 = stars(dir * 2.3 + vec3(100.0));
            float starField3 = stars(dir * 0.5 + vec3(50.0));
            
            // Colored stars
            vec3 starColor1 = vec3(1.0, 1.0, 0.95) * starField1;  // White/yellow
            vec3 starColor2 = vec3(0.8, 0.9, 1.0) * starField2 * 0.7;  // Blue stars
            vec3 starColor3 = vec3(1.0, 0.7, 0.5) * starField3 * 0.5;  // Orange giants
            skyColor += starColor1 + starColor2 + starColor3;
            
            // MILKY WAY GALAXY - band across the sky
            float milkyWayAngle = dir.x * 0.5 + dir.z * 0.866;  // Diagonal band
            float milkyWayDist = abs(dir.y - milkyWayAngle * 0.3);
            float milkyWay = smoothstep(0.4, 0.0, milkyWayDist);
            
            // Milky way texture
            vec3 mwPos = dir * 10.0 + vec3(cloudScroll * 0.5);
            float mwNoise = fbm(mwPos * 3.0) * fbm(mwPos * 7.0 + vec3(10.0));
            milkyWay *= (0.3 + mwNoise * 0.7);
            
            vec3 milkyWayColor = mix(vec3(0.15, 0.1, 0.2), vec3(0.25, 0.2, 0.35), mwNoise);
            milkyWayColor += vec3(0.1, 0.15, 0.25) * stars(dir * 5.0) * 3.0;  // Extra stars in MW
            skyColor += milkyWayColor * milkyWay * 0.6;
            
            // DISTANT GALAXIES - spiral shapes at various positions
            // Galaxy 1 - Large spiral
            vec3 galaxy1Dir = normalize(vec3(0.5, 0.3, -0.7));
            float galaxy1Dot = dot(dir, galaxy1Dir);
            if (galaxy1Dot > 0.97) {
                float gDist = 1.0 - galaxy1Dot;
                float gIntensity = smoothstep(0.03, 0.0, gDist);
                vec2 gUV = vec2(dir.x - galaxy1Dir.x, dir.y - galaxy1Dir.y) * 80.0;
                float spiral = sin(atan(gUV.y, gUV.x) * 2.0 + length(gUV) * 2.0 + uTime * 0.1);
                spiral = spiral * 0.5 + 0.5;
                float gCore = exp(-length(gUV) * 0.5);
                vec3 galaxyColor = mix(vec3(0.8, 0.6, 0.9), vec3(0.4, 0.6, 1.0), spiral);
                skyColor += galaxyColor * gIntensity * (gCore + spiral * 0.3) * 0.5;
            }
            
            // Galaxy 2 - Smaller elliptical
            vec3 galaxy2Dir = normalize(vec3(-0.6, 0.5, 0.4));
            float galaxy2Dot = dot(dir, galaxy2Dir);
            if (galaxy2Dot > 0.985) {
                float gDist = 1.0 - galaxy2Dot;
                float gIntensity = smoothstep(0.015, 0.0, gDist);
                vec3 galaxyColor = vec3(1.0, 0.9, 0.7);
                skyColor += galaxyColor * gIntensity * 0.4;
            }
            
            // NEBULAE - colorful gas clouds
            vec3 nebulaPos = dir * 2.0 + vec3(cloudScroll);
            float nebula1 = fbm(nebulaPos * 2.0);
            float nebula2 = fbm(nebulaPos * 3.0 + vec3(50.0));
            nebula1 = smoothstep(0.4, 0.7, nebula1) * smoothstep(0.9, 0.5, nebula1);
            nebula2 = smoothstep(0.45, 0.75, nebula2) * smoothstep(0.95, 0.55, nebula2);
            
            vec3 nebulaColor1 = vec3(0.8, 0.2, 0.5) * nebula1 * 0.15;  // Pink nebula
            vec3 nebulaColor2 = vec3(0.2, 0.5, 0.9) * nebula2 * 0.12;  // Blue nebula
            skyColor += nebulaColor1 + nebulaColor2;
            
            // THE SUN - bright star in distance
            vec3 sunDir = normalize(vec3(0.8, -0.2, 0.5));
            float sunDot = dot(dir, sunDir);
            if (sunDot > 0.995) {
                float sunDist = 1.0 - sunDot;
                float sunIntensity = smoothstep(0.005, 0.0, sunDist);
                vec3 sunColor = vec3(1.0, 0.95, 0.8);
                skyColor = mix(skyColor, sunColor * 2.0, sunIntensity);
            }
            // Sun glow/corona
            float sunGlow = max(sunDot, 0.0);
            skyColor += vec3(1.0, 0.8, 0.4) * pow(sunGlow, 32.0) * 0.8;
            skyColor += vec3(1.0, 0.6, 0.2) * pow(sunGlow, 8.0) * 0.3;
            
            // PLANETS
            // Earth - blue marble
            vec3 earthDir = normalize(vec3(-0.3, 0.1, 0.8));
            float earthDot = dot(dir, earthDir);
            if (earthDot > 0.992) {
                float eDist = 1.0 - earthDot;
                float eIntensity = smoothstep(0.008, 0.001, eDist);
                vec2 eUV = vec2(dir.x - earthDir.x, dir.y - earthDir.y) * 150.0;
                float continents = fbm(vec3(eUV + uTime * 0.02, 0.0));
                vec3 earthColor = mix(vec3(0.1, 0.3, 0.8), vec3(0.2, 0.6, 0.3), smoothstep(0.4, 0.6, continents));
                earthColor = mix(earthColor, vec3(1.0), smoothstep(0.7, 0.75, continents) * 0.8);  // Clouds
                skyColor = mix(skyColor, earthColor, eIntensity);
            }
            
            // Mars - red planet
            vec3 marsDir = normalize(vec3(0.6, 0.4, -0.5));
            float marsDot = dot(dir, marsDir);
            if (marsDot > 0.994) {
                float mDist = 1.0 - marsDot;
                float mIntensity = smoothstep(0.006, 0.001, mDist);
                vec2 mUV = vec2(dir.x - marsDir.x, dir.y - marsDir.y) * 200.0;
                float terrain = fbm(vec3(mUV, 0.0)) * 0.3;
                vec3 marsColor = vec3(0.8, 0.4, 0.2) + terrain * vec3(0.2, 0.1, 0.05);
                skyColor = mix(skyColor, marsColor, mIntensity);
            }
            
            // Jupiter - gas giant with bands
            vec3 jupiterDir = normalize(vec3(-0.7, -0.3, -0.4));
            float jupiterDot = dot(dir, jupiterDir);
            if (jupiterDot > 0.985) {
                float jDist = 1.0 - jupiterDot;
                float jIntensity = smoothstep(0.015, 0.002, jDist);
                vec2 jUV = vec2(dir.x - jupiterDir.x, dir.y - jupiterDir.y) * 60.0;
                float bands = sin(jUV.y * 15.0 + fbm(vec3(jUV * 2.0, uTime * 0.1)) * 2.0) * 0.5 + 0.5;
                vec3 jupiterColor = mix(vec3(0.8, 0.7, 0.5), vec3(0.9, 0.6, 0.4), bands);
                // Great red spot
                float spot = smoothstep(0.5, 0.0, length(jUV - vec2(1.0, 0.5)));
                jupiterColor = mix(jupiterColor, vec3(0.9, 0.4, 0.3), spot * 0.7);
                skyColor = mix(skyColor, jupiterColor, jIntensity);
            }
            
            // Saturn - with rings!
            vec3 saturnDir = normalize(vec3(0.2, 0.6, 0.7));
            float saturnDot = dot(dir, saturnDir);
            if (saturnDot > 0.98) {
                float sDist = 1.0 - saturnDot;
                float sIntensity = smoothstep(0.02, 0.005, sDist);
                vec2 sUV = vec2(dir.x - saturnDir.x, dir.z - saturnDir.z) * 50.0;
                float sUVy = (dir.y - saturnDir.y) * 50.0;
                
                // Planet body
                float planetDist = length(vec2(sUV.x, sUVy * 2.5));
                float planet = smoothstep(1.2, 0.8, planetDist);
                vec3 saturnColor = vec3(0.9, 0.85, 0.6);
                
                // Rings
                float ringDist = length(vec2(sUV.x, sUVy * 5.0));
                float rings = smoothstep(1.5, 1.6, ringDist) * smoothstep(3.0, 2.8, ringDist);
                rings *= (sin(ringDist * 20.0) * 0.3 + 0.7);  // Ring bands
                rings *= step(0.3, abs(sUVy / (ringDist + 0.01)));  // Hide behind planet
                vec3 ringColor = vec3(0.8, 0.75, 0.6);
                
                vec3 finalSaturn = mix(ringColor * rings, saturnColor, planet);
                float saturnAlpha = max(planet, rings * 0.7);
                skyColor = mix(skyColor, finalSaturn, sIntensity * saturnAlpha);
            }
            
            // Shooting stars / meteors (occasional)
            float meteorTime = floor(uTime * 0.3);
            vec3 meteorDir = normalize(vec3(
                sin(meteorTime * 12.34) * 0.5,
                cos(meteorTime * 23.45) * 0.3 + 0.5,
                sin(meteorTime * 34.56) * 0.5
            ));
            float meteorProgress = fract(uTime * 0.3);
            vec3 meteorPos = meteorDir - normalize(vec3(1.0, -0.5, 0.5)) * meteorProgress * 0.3;
            float meteorDot = dot(dir, normalize(meteorPos));
            if (meteorDot > 0.9995 && meteorProgress < 0.5) {
                skyColor += vec3(1.0, 0.9, 0.7) * (1.0 - meteorProgress * 2.0) * 2.0;
            }
            
            return skyColor;
        }
        
        vec3 mirrorWorld(vec3 dir) {
            float y = dir.y;
            float cloudScroll = uFallDistance * 0.001;
            
            // Surreal chrome/mirror sky
            vec3 skyTop = vec3(0.6, 0.65, 0.7);       // Silver
            vec3 skyMid = vec3(0.8, 0.82, 0.85);      // Bright silver
            vec3 horizon = vec3(0.9, 0.85, 0.95);     // Pinkish silver
            vec3 skyBottom = vec3(0.5, 0.55, 0.65);   // Darker silver
            
            vec3 skyColor;
            if (y > 0.0) {
                float t = pow(y, 0.5);
                skyColor = mix(mix(horizon, skyMid, smoothstep(0.0, 0.3, y)), skyTop, t);
            } else {
                skyColor = mix(horizon, skyBottom, smoothstep(0.0, -0.5, y));
            }
            
            // Floating mirror panels - create grid pattern
            vec3 absDir = abs(dir);
            float gridX = sin(dir.x * 8.0 + cloudScroll * 2.0) * cos(dir.z * 8.0);
            float gridZ = sin(dir.z * 8.0 + cloudScroll * 2.0) * cos(dir.x * 8.0);
            float gridY = sin(dir.y * 6.0 + uTime * 0.5);
            
            // Mirror reflections - simulate reflective surfaces
            float mirrors = smoothstep(0.7, 0.9, abs(gridX * gridZ));
            mirrors *= smoothstep(-0.3, 0.3, y);  // More mirrors at eye level
            
            // Rainbow chromatic effect on mirrors
            vec3 chromeColor;
            float angle = atan(dir.z, dir.x) + uTime * 0.2;
            chromeColor.r = sin(angle * 3.0) * 0.5 + 0.5;
            chromeColor.g = sin(angle * 3.0 + 2.094) * 0.5 + 0.5;
            chromeColor.b = sin(angle * 3.0 + 4.188) * 0.5 + 0.5;
            chromeColor = mix(vec3(0.9), chromeColor, 0.4);
            
            // Reflection distortion
            vec3 reflectDir = reflect(dir, normalize(vec3(sin(uTime * 0.3), 1.0, cos(uTime * 0.2))));
            float reflectPattern = fbm(reflectDir * 5.0 + cloudScroll);
            
            skyColor = mix(skyColor, chromeColor, mirrors * 0.8);
            skyColor += vec3(1.0) * mirrors * reflectPattern * 0.3;
            
            // Floating geometric shapes (mirror frames)
            float shapes = 0.0;
            for (int i = 0; i < 5; i++) {
                vec3 shapeCenter = vec3(
                    sin(float(i) * 1.5 + uTime * 0.1) * 0.5,
                    cos(float(i) * 2.0 + cloudScroll * 0.5) * 0.3,
                    sin(float(i) * 0.8 + uTime * 0.15) * 0.5
                );
                float d = length(dir - shapeCenter);
                float ring = smoothstep(0.12, 0.1, d) - smoothstep(0.1, 0.08, d);
                shapes += ring;
            }
            skyColor += vec3(0.9, 0.95, 1.0) * shapes * 0.8;
            
            // Ambient glow
            float glow = pow(1.0 - abs(y), 2.0);
            skyColor += vec3(0.8, 0.75, 0.9) * glow * 0.15;
            
            // Subtle noise texture
            float n = noise(dir * 20.0 + uTime * 0.1);
            skyColor += vec3(n * 0.05);
            
            return skyColor;
        }
        
        vec3 speedforceWorld(vec3 dir) {
            float y = dir.y;
            float cloudScroll = uFallDistance * 0.005;  // Faster scrolling!
            
            // Slow color breathing/pulsing effect
            float colorPulse = sin(uTime * 0.5) * 0.5 + 0.5;  // Slow oscillation 0-1
            
            // Electric red/orange/yellow energy background
            // Original colors
            vec3 skyTop1 = vec3(0.6, 0.1, 0.0);        // Deep red
            vec3 skyMid1 = vec3(0.9, 0.3, 0.0);        // Orange
            vec3 horizon1 = vec3(1.0, 0.7, 0.1);       // Yellow-orange
            vec3 skyBottom1 = vec3(0.4, 0.05, 0.0);    // Dark red
            
            // Alternate colors (slightly different hues)
            vec3 skyTop2 = vec3(0.5, 0.15, 0.05);      // Darker red-brown
            vec3 skyMid2 = vec3(0.8, 0.4, 0.1);        // Warmer orange
            vec3 horizon2 = vec3(1.0, 0.6, 0.2);       // More orange
            vec3 skyBottom2 = vec3(0.35, 0.1, 0.05);   // Warmer dark
            
            // Blend between color sets based on pulse
            vec3 skyTop = mix(skyTop1, skyTop2, colorPulse);
            vec3 skyMid = mix(skyMid1, skyMid2, colorPulse);
            vec3 horizon = mix(horizon1, horizon2, colorPulse);
            vec3 skyBottom = mix(skyBottom1, skyBottom2, colorPulse);
            
            vec3 skyColor;
            if (y > 0.0) {
                float t = pow(y, 0.5);
                skyColor = mix(mix(horizon, skyMid, smoothstep(0.0, 0.3, y)), skyTop, t);
            } else {
                skyColor = mix(horizon, skyBottom, smoothstep(0.0, -0.5, y));
            }
            
            // Speed lines / energy streaks
            float speedLines = 0.0;
            for (int i = 0; i < 20; i++) {
                float fi = float(i);
                float angle = fi * 0.314159 + cloudScroll * 10.0;
                vec2 lineDir = vec2(cos(angle), sin(angle));
                float line = abs(dot(dir.xz, lineDir));
                line = pow(line, 50.0);
                float flicker = sin(uTime * 20.0 + fi * 5.0) * 0.5 + 0.5;
                speedLines += line * flicker * 0.15;
            }
            skyColor += vec3(1.0, 0.8, 0.3) * speedLines;
            
            // Lightning bolts
            float lightning = 0.0;
            for (int i = 0; i < 8; i++) {
                float fi = float(i);
                vec3 boltOrigin = vec3(
                    sin(fi * 2.4 + uTime * 0.5) * 0.8,
                    cos(fi * 1.7 + cloudScroll) * 0.5,
                    sin(fi * 3.1 + uTime * 0.3) * 0.8
                );
                
                // Jagged bolt path
                vec3 boltDir = normalize(dir - boltOrigin);
                float boltDist = length(dir - boltOrigin);
                
                // Create jagged effect
                float jagged = sin(boltDist * 30.0 + fi * 10.0 + uTime * 15.0) * 0.02;
                float bolt = smoothstep(0.08 + jagged, 0.0, boltDist);
                
                // Branching
                float branch1 = smoothstep(0.12, 0.0, length(dir - boltOrigin - vec3(0.1, 0.1, 0.0)));
                float branch2 = smoothstep(0.1, 0.0, length(dir - boltOrigin - vec3(-0.08, 0.15, 0.05)));
                
                float flicker = step(0.7, sin(uTime * 30.0 + fi * 20.0) * 0.5 + 0.5);
                lightning += (bolt + branch1 * 0.5 + branch2 * 0.3) * flicker;
            }
            
            // Lightning glow color (white-yellow core, orange edge)
            vec3 lightningColor = mix(vec3(1.0, 0.9, 0.5), vec3(1.0, 1.0, 1.0), lightning);
            skyColor += lightningColor * lightning * 2.0;
            
            // Electric arcs / plasma effect
            float plasma = 0.0;
            plasma += sin(dir.x * 15.0 + uTime * 8.0) * sin(dir.y * 12.0 - cloudScroll * 20.0);
            plasma += sin(dir.z * 18.0 - uTime * 6.0) * sin(dir.x * 10.0 + uTime * 4.0);
            plasma = plasma * 0.25 + 0.5;
            plasma = pow(plasma, 3.0);
            skyColor += vec3(1.0, 0.5, 0.1) * plasma * 0.3;
            
            // Energy vortex effect (tunnel feeling)
            float vortex = atan(dir.z, dir.x) + cloudScroll * 5.0;
            float vortexRings = sin(vortex * 8.0 + length(dir.xz) * 10.0 - uTime * 10.0);
            vortexRings = smoothstep(0.8, 1.0, vortexRings);
            skyColor += vec3(1.0, 0.6, 0.0) * vortexRings * 0.4;
            
            // Particle streaks (blur effect)
            float streaks = noise(vec3(dir.xy * 5.0, cloudScroll * 50.0));
            streaks = smoothstep(0.6, 1.0, streaks);
            skyColor += vec3(1.0, 0.9, 0.6) * streaks * 0.2;
            
            // ===========================================
            // FLASH VS REVERSE FLASH RACING STREAKS
            // ===========================================
            // The Flash - Blue lightning streaks
            // Reverse Flash - Red lightning streaks
            // Racing along the horizon!
            
            float horizonBand = smoothstep(0.25, 0.0, abs(y)) * smoothstep(-0.25, 0.0, y + 0.15);
            
            // The Flash (Barry Allen) - Blue streak racing left to right
            float flashAngle = atan(dir.z, dir.x);
            float flashSpeed = uTime * 5.0 + cloudScroll * 10.0;  // Fast!
            
            // Main Flash streak - thin and sharp
            float flashStreak = 0.0;
            for (int i = 0; i < 2; i++) {
                float fi = float(i);
                float offset = fi * 0.5;
                float streak = sin(flashAngle * 2.0 + flashSpeed + offset) * 0.5 + 0.5;
                streak = pow(streak, 80.0);  // Much sharper/thinner streak
                float yOffset = sin(flashAngle * 3.0 + uTime * 6.0 + fi) * 0.02;
                float verticalBand = smoothstep(0.04, 0.0, abs(y - 0.02 + yOffset - fi * 0.03));  // Thinner band
                flashStreak += streak * verticalBand * (1.0 - fi * 0.3);
            }
            
            // Blue lightning color for The Flash
            vec3 flashColor = vec3(0.2, 0.5, 1.0);  // Electric blue
            vec3 flashCore = vec3(0.8, 0.95, 1.0);  // White-blue core
            skyColor += mix(flashColor, flashCore, flashStreak) * flashStreak * horizonBand * 2.0;
            
            // Reverse Flash (Eobard Thawne) - Red streak racing opposite direction
            float rfSpeed = -uTime * 5.3 - cloudScroll * 10.5;  // Slightly faster, opposite direction!
            
            // Main Reverse Flash streak - thin and sharp
            float rfStreak = 0.0;
            for (int i = 0; i < 2; i++) {
                float fi = float(i);
                float offset = fi * 0.4;
                float streak = sin(flashAngle * 2.0 + rfSpeed + offset + 3.14159) * 0.5 + 0.5;  // Opposite phase
                streak = pow(streak, 80.0);  // Much sharper/thinner streak
                float yOffset = sin(flashAngle * 3.0 - uTime * 5.0 + fi) * 0.02;
                float verticalBand = smoothstep(0.04, 0.0, abs(y + 0.02 + yOffset - fi * 0.03));  // Thinner band
                rfStreak += streak * verticalBand * (1.0 - fi * 0.3);
            }
            
            // Red lightning color for Reverse Flash
            vec3 rfColor = vec3(1.0, 0.1, 0.0);    // Electric red
            vec3 rfCore = vec3(1.0, 0.6, 0.2);     // Yellow-orange core
            skyColor += mix(rfColor, rfCore, rfStreak) * rfStreak * horizonBand * 2.0;
            
            // Collision sparks when they pass each other
            float collision = flashStreak * rfStreak * 15.0;
            skyColor += vec3(1.0, 1.0, 0.8) * collision * horizonBand;
            
            // Central bright core
            float core = 1.0 - length(dir.xz);
            core = pow(max(core, 0.0), 2.0);
            skyColor += vec3(1.0, 0.95, 0.8) * core * 0.3;
            
            // Flash effect from uniform
            skyColor += vec3(1.0, 0.9, 0.7) * uTransition * 2.0;
            
            return skyColor;
        }
        
        vec3 hallOfMirrorsWorld(vec3 dir) {
            float y = dir.y;
            
            // Dark elegant hall atmosphere
            vec3 skyTop = vec3(0.05, 0.03, 0.08);      // Very dark purple
            vec3 skyMid = vec3(0.1, 0.08, 0.15);       // Dark purple
            vec3 horizon = vec3(0.15, 0.1, 0.2);      // Purple horizon
            vec3 skyBottom = vec3(0.02, 0.02, 0.05);  // Almost black
            
            vec3 skyColor;
            if (y > 0.0) {
                float t = pow(y, 0.5);
                skyColor = mix(mix(horizon, skyMid, smoothstep(0.0, 0.3, y)), skyTop, t);
            } else {
                skyColor = mix(horizon, skyBottom, smoothstep(0.0, -0.5, y));
            }
            
            // Ambient golden light sources (like chandeliers)
            float lights = 0.0;
            for (int i = 0; i < 4; i++) {
                vec3 lightPos = vec3(
                    sin(float(i) * 1.57) * 0.3,
                    0.6 + float(i) * 0.1,
                    cos(float(i) * 1.57) * 0.3
                );
                float d = length(dir - normalize(lightPos));
                lights += smoothstep(0.15, 0.0, d) * 0.5;
                lights += smoothstep(0.4, 0.1, d) * 0.2;
            }
            skyColor += vec3(1.0, 0.8, 0.4) * lights;
            
            // Subtle fog/mist
            float fog = pow(1.0 - abs(y), 4.0);
            skyColor = mix(skyColor, vec3(0.15, 0.12, 0.2), fog * 0.3);
            
            return skyColor;
        }
        
        void main() {
            vec3 dir = normalize(vDir);
            
            vec3 dayColor = dayWorld(dir);
            vec3 spaceColor = spaceWorld(dir);
            vec3 mirrorColor = mirrorWorld(dir);
            vec3 speedforceColor = speedforceWorld(dir);
            vec3 hallColor = hallOfMirrorsWorld(dir);
            
            // Select world color based on world type
            vec3 finalColor;
            if (uWorldType == 0) {
                finalColor = dayColor;
            } else if (uWorldType == 1) {
                finalColor = spaceColor;
            } else if (uWorldType == 2) {
                finalColor = mirrorColor;
            } else if (uWorldType == 3) {
                finalColor = speedforceColor;
            } else {
                finalColor = hallColor;
            }
            
            // Portal transition flash effect
            if (uTransition > 0.0) {
                vec3 flashColor;
                if (uWorldType == 2) flashColor = vec3(1.5, 1.5, 1.8);
                else if (uWorldType == 3) flashColor = vec3(2.0, 1.5, 0.5);
                else if (uWorldType == 4) flashColor = vec3(1.0, 0.8, 1.2);
                else flashColor = vec3(1.0, 1.0, 1.5);
                finalColor = mix(finalColor, flashColor, uTransition * uTransition);
            }
            
            FragColor = vec4(finalColor, 1.0);
        }
    )";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSource, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSource, nullptr);
    glCompileShader(fs);

    skyboxProgram = glCreateProgram();
    glAttachShader(skyboxProgram, vs);
    glAttachShader(skyboxProgram, fs);
    glLinkProgram(skyboxProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);
}

// ---------------------------------------------------------------------------
// Character (tumbling humanoid shape)
// ---------------------------------------------------------------------------
static GLuint characterVAO = 0, characterVBO = 0, characterEBO = 0;
static GLuint characterProgram = 0;
static int characterIndexCount = 0;

static void AddBox(std::vector<float>& verts, std::vector<unsigned int>& inds,
                   float x, float y, float z, float sx, float sy, float sz,
                   float r, float g, float b) {
    unsigned int baseIdx = (unsigned int)(verts.size() / 9);
    
    // 8 corners of the box
    float corners[8][3] = {
        {x - sx, y - sy, z - sz},
        {x + sx, y - sy, z - sz},
        {x + sx, y + sy, z - sz},
        {x - sx, y + sy, z - sz},
        {x - sx, y - sy, z + sz},
        {x + sx, y - sy, z + sz},
        {x + sx, y + sy, z + sz},
        {x - sx, y + sy, z + sz}
    };
    
    // 6 faces with normals
    int faces[6][4] = {
        {0, 1, 2, 3}, // back
        {5, 4, 7, 6}, // front
        {4, 0, 3, 7}, // left
        {1, 5, 6, 2}, // right
        {3, 2, 6, 7}, // top
        {4, 5, 1, 0}  // bottom
    };
    float normals[6][3] = {
        {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}
    };
    
    for (int f = 0; f < 6; f++) {
        unsigned int idx = baseIdx + f * 4;
        for (int v = 0; v < 4; v++) {
            int c = faces[f][v];
            verts.push_back(corners[c][0]);
            verts.push_back(corners[c][1]);
            verts.push_back(corners[c][2]);
            verts.push_back(normals[f][0]);
            verts.push_back(normals[f][1]);
            verts.push_back(normals[f][2]);
            verts.push_back(r);
            verts.push_back(g);
            verts.push_back(b);
        }
        // Two triangles per face
        inds.push_back(idx + 0);
        inds.push_back(idx + 1);
        inds.push_back(idx + 2);
        inds.push_back(idx + 0);
        inds.push_back(idx + 2);
        inds.push_back(idx + 3);
    }
}

// ---------------------------------------------------------------------------
// Animated Simple Character
// ---------------------------------------------------------------------------
// Store separate VAOs for each body part so we can animate them
static GLuint bodyPartVAOs[6];  // torso, head, leftArm, rightArm, leftLeg, rightLeg
static GLuint bodyPartVBOs[6];
static GLuint bodyPartEBOs[6];
static int bodyPartIndexCounts[6];

// Smooth animation state for simple character (current interpolated values)
static float currentBodyTilt = -90.0f;   // Start facing down (diving) - head pointing down towards ground
static float currentArmSwing = 0.0f;
static float currentArmSpread = 30.0f;  // Arms slightly spread in idle
static float currentLegSwing = 0.0f;
static float currentLegSpread = 10.0f;  // Legs slightly apart in idle

static void CreateBodyPart(int partIndex, float cx, float cy, float cz, float hw, float hh, float hd, float r, float g, float b) {
    std::vector<float> verts;
    std::vector<unsigned int> inds;
    
    // Create box centered at origin (we'll position it with transforms)
    float corners[8][3] = {
        {-hw, -hh, -hd}, {hw, -hh, -hd}, {hw, hh, -hd}, {-hw, hh, -hd},
        {-hw, -hh, hd}, {hw, -hh, hd}, {hw, hh, hd}, {-hw, hh, hd}
    };
    
    int faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}
    };
    float normals[6][3] = {
        {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}
    };
    
    for (int f = 0; f < 6; f++) {
        unsigned int idx = f * 4;
        for (int v = 0; v < 4; v++) {
            int c = faces[f][v];
            verts.push_back(corners[c][0]);
            verts.push_back(corners[c][1]);
            verts.push_back(corners[c][2]);
            verts.push_back(normals[f][0]);
            verts.push_back(normals[f][1]);
            verts.push_back(normals[f][2]);
            verts.push_back(r);
            verts.push_back(g);
            verts.push_back(b);
        }
        inds.push_back(idx + 0);
        inds.push_back(idx + 1);
        inds.push_back(idx + 2);
        inds.push_back(idx + 0);
        inds.push_back(idx + 2);
        inds.push_back(idx + 3);
    }
    
    bodyPartIndexCounts[partIndex] = (int)inds.size();
    
    glGenVertexArrays(1, &bodyPartVAOs[partIndex]);
    glGenBuffers(1, &bodyPartVBOs[partIndex]);
    glGenBuffers(1, &bodyPartEBOs[partIndex]);
    
    glBindVertexArray(bodyPartVAOs[partIndex]);
    glBindBuffer(GL_ARRAY_BUFFER, bodyPartVBOs[partIndex]);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bodyPartEBOs[partIndex]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    
    glBindVertexArray(0);
}

static void InitCharacter() {
    // Create separate body parts for animation
    // Part 0: Torso (centered at origin)
    CreateBodyPart(0, 0, 0, 0, 0.4f, 0.6f, 0.2f, 0.2f, 0.4f, 0.8f);
    
    // Part 1: Head
    CreateBodyPart(1, 0, 0, 0, 0.25f, 0.25f, 0.25f, 0.9f, 0.75f, 0.6f);
    
    // Part 2: Left arm
    CreateBodyPart(2, 0, 0, 0, 0.12f, 0.4f, 0.12f, 0.2f, 0.4f, 0.8f);
    
    // Part 3: Right arm
    CreateBodyPart(3, 0, 0, 0, 0.12f, 0.4f, 0.12f, 0.2f, 0.4f, 0.8f);
    
    // Part 4: Left leg
    CreateBodyPart(4, 0, 0, 0, 0.15f, 0.5f, 0.15f, 0.3f, 0.3f, 0.5f);
    
    // Part 5: Right leg
    CreateBodyPart(5, 0, 0, 0, 0.15f, 0.5f, 0.15f, 0.3f, 0.3f, 0.5f);
    
    // Also create the old combined mesh for backwards compatibility
    std::vector<float> verts;
    std::vector<unsigned int> inds;
    
    // Build a simple humanoid from boxes
    // Body (torso)
    AddBox(verts, inds, 0, 0, 0, 0.4f, 0.6f, 0.2f, 0.2f, 0.4f, 0.8f);
    
    // Head
    AddBox(verts, inds, 0, 0.9f, 0, 0.25f, 0.25f, 0.25f, 0.9f, 0.75f, 0.6f);
    
    // Left arm
    AddBox(verts, inds, -0.6f, 0.1f, 0, 0.15f, 0.5f, 0.15f, 0.2f, 0.4f, 0.8f);
    
    // Right arm
    AddBox(verts, inds, 0.6f, 0.1f, 0, 0.15f, 0.5f, 0.15f, 0.2f, 0.4f, 0.8f);
    
    // Left leg
    AddBox(verts, inds, -0.2f, -1.1f, 0, 0.15f, 0.5f, 0.15f, 0.3f, 0.3f, 0.5f);
    
    // Right leg
    AddBox(verts, inds, 0.2f, -1.1f, 0, 0.15f, 0.5f, 0.15f, 0.3f, 0.3f, 0.5f);
    
    characterIndexCount = (int)inds.size();
    
    glGenVertexArrays(1, &characterVAO);
    glGenBuffers(1, &characterVBO);
    glGenBuffers(1, &characterEBO);
    
    glBindVertexArray(characterVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, characterVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, characterEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    // Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    // Color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    
    glBindVertexArray(0);
    
    // Character shader
    const char* vsSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec3 aColor;
        
        out vec3 vNormal;
        out vec3 vColor;
        out vec3 vWorldPos;
        
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;
        
        void main() {
            vec4 worldPos = uModel * vec4(aPos, 1.0);
            vWorldPos = worldPos.xyz;
            vNormal = mat3(transpose(inverse(uModel))) * aNormal;
            vColor = aColor;
            gl_Position = uProj * uView * worldPos;
        }
    )";
    
    const char* fsSource = R"(
        #version 330 core
        in vec3 vNormal;
        in vec3 vColor;
        in vec3 vWorldPos;
        
        out vec4 FragColor;
        
        uniform vec3 uLightDir;
        uniform vec3 uViewPos;
        uniform float uReflection;  // 0 = normal, >0 = reflection (dimmer)
        uniform int uMirrorClipMode;  // 0=none, 1=left, 2=right, 3=front, 4=back
        uniform float uMirrorBounds;  // Half-width of mirror
        uniform vec3 uCharacterPos;   // Original character position for clipping calc
        
        void main() {
            // Mirror clipping - fade out parts outside mirror bounds
            float clipAlpha = 1.0;
            if (uMirrorClipMode > 0 && uReflection > 0.0) {
                // Calculate how far the reflection point is from mirror center
                float distFromCenter = 0.0;
                if (uMirrorClipMode == 1 || uMirrorClipMode == 2) {
                    // Left/Right mirrors - check Z bounds
                    distFromCenter = abs(uCharacterPos.z);
                } else if (uMirrorClipMode == 3 || uMirrorClipMode == 4) {
                    // Front/Back mirrors - check X bounds
                    distFromCenter = abs(uCharacterPos.x);
                }
                
                // Soft fade at edges
                float edgeDist = uMirrorBounds - distFromCenter;
                if (edgeDist < 0.0) {
                    discard;  // Completely outside
                } else if (edgeDist < 3.0) {
                    clipAlpha = edgeDist / 3.0;  // Fade in the last 3 units
                }
            }
            
            vec3 N = normalize(vNormal);
            vec3 L = normalize(uLightDir);
            
            // Ambient
            vec3 ambient = 0.3 * vColor;
            
            // Diffuse
            float diff = max(dot(N, L), 0.0);
            vec3 diffuse = diff * vColor;
            
            // Specular
            vec3 V = normalize(uViewPos - vWorldPos);
            vec3 H = normalize(L + V);
            float spec = pow(max(dot(N, H), 0.0), 32.0);
            vec3 specular = vec3(0.3) * spec;
            
            // Rim light (backlight effect)
            float rim = 1.0 - max(dot(N, V), 0.0);
            rim = pow(rim, 3.0);
            vec3 rimColor = vec3(0.5, 0.6, 0.8) * rim * 0.5;
            
            vec3 color = ambient + diffuse + specular + rimColor;
            
            // Apply reflection dimming
            if (uReflection > 0.0) {
                color *= (1.0 - uReflection * 0.5);  // Dim reflection
                color = mix(color, vec3(0.7, 0.8, 0.9), 0.2);  // Slight blue tint
            }
            
            FragColor = vec4(color, clipAlpha);
        }
    )";
    
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSource, nullptr);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSource, nullptr);
    glCompileShader(fs);
    
    characterProgram = glCreateProgram();
    glAttachShader(characterProgram, vs);
    glAttachShader(characterProgram, fs);
    glLinkProgram(characterProgram);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
}

// ---------------------------------------------------------------------------
// Bird Bot Initialization
// ---------------------------------------------------------------------------
static void CreateBirdMeshes() {
    // Bird body - small elongated ellipsoid-like shape
    std::vector<float> bodyVerts;
    std::vector<unsigned int> bodyInds;
    
    // Simple bird body (elongated box)
    float bw = 0.15f, bh = 0.12f, bl = 0.4f;  // width, height, length
    float corners[8][3] = {
        {-bw, -bh, -bl}, {bw, -bh, -bl}, {bw, bh, -bl}, {-bw, bh, -bl},
        {-bw, -bh, bl}, {bw, -bh, bl}, {bw, bh, bl}, {-bw, bh, bl}
    };
    int faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}
    };
    float normals[6][3] = {
        {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}
    };
    // Bird body color - dark gray/black
    float br = 0.2f, bg = 0.2f, bb = 0.25f;
    
    for (int f = 0; f < 6; f++) {
        unsigned int idx = f * 4;
        for (int v = 0; v < 4; v++) {
            int c = faces[f][v];
            bodyVerts.push_back(corners[c][0]);
            bodyVerts.push_back(corners[c][1]);
            bodyVerts.push_back(corners[c][2]);
            bodyVerts.push_back(normals[f][0]);
            bodyVerts.push_back(normals[f][1]);
            bodyVerts.push_back(normals[f][2]);
            bodyVerts.push_back(br);
            bodyVerts.push_back(bg);
            bodyVerts.push_back(bb);
        }
        bodyInds.push_back(idx + 0);
        bodyInds.push_back(idx + 1);
        bodyInds.push_back(idx + 2);
        bodyInds.push_back(idx + 0);
        bodyInds.push_back(idx + 2);
        bodyInds.push_back(idx + 3);
    }
    
    // Add a beak (small cone-like triangle at front)
    // Beak vertices (pointing forward in -Z direction)
    unsigned int beakStart = (unsigned int)(bodyVerts.size() / 9);
    float beakLen = 0.2f;
    // Top triangle
    bodyVerts.insert(bodyVerts.end(), {0, 0.05f, -bl - beakLen, 0, 0.5f, -0.86f, 1.0f, 0.6f, 0.1f});
    bodyVerts.insert(bodyVerts.end(), {-0.05f, -0.02f, -bl, 0, 0.5f, -0.86f, 1.0f, 0.6f, 0.1f});
    bodyVerts.insert(bodyVerts.end(), {0.05f, -0.02f, -bl, 0, 0.5f, -0.86f, 1.0f, 0.6f, 0.1f});
    bodyInds.push_back(beakStart); bodyInds.push_back(beakStart + 1); bodyInds.push_back(beakStart + 2);
    
    birdBodyIndexCount = (int)bodyInds.size();
    
    glGenVertexArrays(1, &birdBodyVAO);
    glGenBuffers(1, &birdBodyVBO);
    glGenBuffers(1, &birdBodyEBO);
    glBindVertexArray(birdBodyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, birdBodyVBO);
    glBufferData(GL_ARRAY_BUFFER, bodyVerts.size() * sizeof(float), bodyVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, birdBodyEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bodyInds.size() * sizeof(unsigned int), bodyInds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    
    // Bird wing - flat triangular shape
    std::vector<float> wingVerts;
    std::vector<unsigned int> wingInds;
    
    // Wing is a flat triangle, will be rotated for flapping
    // Pivot at body connection, extends outward
    float wingSpan = 0.5f, wingDepth = 0.3f;
    // Wing color - slightly lighter
    float wr = 0.25f, wg = 0.25f, wb = 0.3f;
    
    // Top face of wing
    wingVerts.insert(wingVerts.end(), {0, 0, 0, 0, 1, 0, wr, wg, wb});  // Pivot point
    wingVerts.insert(wingVerts.end(), {wingSpan, 0, -wingDepth * 0.3f, 0, 1, 0, wr, wg, wb});  // Tip front
    wingVerts.insert(wingVerts.end(), {wingSpan * 0.8f, 0, wingDepth * 0.5f, 0, 1, 0, wr, wg, wb});  // Tip back
    wingInds.push_back(0); wingInds.push_back(1); wingInds.push_back(2);
    
    // Bottom face (flipped normal)
    wingVerts.insert(wingVerts.end(), {0, 0, 0, 0, -1, 0, wr, wg, wb});
    wingVerts.insert(wingVerts.end(), {wingSpan, 0, -wingDepth * 0.3f, 0, -1, 0, wr, wg, wb});
    wingVerts.insert(wingVerts.end(), {wingSpan * 0.8f, 0, wingDepth * 0.5f, 0, -1, 0, wr, wg, wb});
    wingInds.push_back(3); wingInds.push_back(5); wingInds.push_back(4);
    
    birdWingIndexCount = (int)wingInds.size();
    
    glGenVertexArrays(1, &birdWingVAO);
    glGenBuffers(1, &birdWingVBO);
    glGenBuffers(1, &birdWingEBO);
    glBindVertexArray(birdWingVAO);
    glBindBuffer(GL_ARRAY_BUFFER, birdWingVBO);
    glBufferData(GL_ARRAY_BUFFER, wingVerts.size() * sizeof(float), wingVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, birdWingEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, wingInds.size() * sizeof(unsigned int), wingInds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
}

static void CreateAirplaneMeshes() {
    // Airplane body - long cylinder-like fuselage
    std::vector<float> bodyVerts;
    std::vector<unsigned int> bodyInds;
    
    // Fuselage (elongated box for simplicity)
    float fw = 0.8f, fh = 0.6f, fl = 4.0f;
    float corners[8][3] = {
        {-fw, -fh, -fl}, {fw, -fh, -fl}, {fw, fh, -fl}, {-fw, fh, -fl},
        {-fw, -fh, fl}, {fw, -fh, fl}, {fw, fh, fl}, {-fw, fh, fl}
    };
    int faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}
    };
    float normals[6][3] = {
        {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}
    };
    // Airplane body color - white/silver
    float ar = 0.9f, ag = 0.9f, ab = 0.95f;
    
    for (int f = 0; f < 6; f++) {
        unsigned int idx = f * 4;
        for (int v = 0; v < 4; v++) {
            int c = faces[f][v];
            bodyVerts.push_back(corners[c][0]);
            bodyVerts.push_back(corners[c][1]);
            bodyVerts.push_back(corners[c][2]);
            bodyVerts.push_back(normals[f][0]);
            bodyVerts.push_back(normals[f][1]);
            bodyVerts.push_back(normals[f][2]);
            bodyVerts.push_back(ar);
            bodyVerts.push_back(ag);
            bodyVerts.push_back(ab);
        }
        bodyInds.push_back(idx + 0);
        bodyInds.push_back(idx + 1);
        bodyInds.push_back(idx + 2);
        bodyInds.push_back(idx + 0);
        bodyInds.push_back(idx + 2);
        bodyInds.push_back(idx + 3);
    }
    
    // Nose cone
    unsigned int noseStart = (unsigned int)(bodyVerts.size() / 9);
    float noseLen = 1.5f;
    // Top
    bodyVerts.insert(bodyVerts.end(), {0, 0, -fl - noseLen, 0, 0.3f, -0.95f, ar, ag, ab});
    bodyVerts.insert(bodyVerts.end(), {-fw, fh, -fl, 0, 0.3f, -0.95f, ar, ag, ab});
    bodyVerts.insert(bodyVerts.end(), {fw, fh, -fl, 0, 0.3f, -0.95f, ar, ag, ab});
    bodyInds.push_back(noseStart); bodyInds.push_back(noseStart + 1); bodyInds.push_back(noseStart + 2);
    // Bottom
    bodyVerts.insert(bodyVerts.end(), {0, 0, -fl - noseLen, 0, -0.3f, -0.95f, ar, ag, ab});
    bodyVerts.insert(bodyVerts.end(), {fw, -fh, -fl, 0, -0.3f, -0.95f, ar, ag, ab});
    bodyVerts.insert(bodyVerts.end(), {-fw, -fh, -fl, 0, -0.3f, -0.95f, ar, ag, ab});
    bodyInds.push_back(noseStart + 3); bodyInds.push_back(noseStart + 4); bodyInds.push_back(noseStart + 5);
    
    airplaneBodyIndexCount = (int)bodyInds.size();
    
    glGenVertexArrays(1, &airplaneBodyVAO);
    glGenBuffers(1, &airplaneBodyVBO);
    glGenBuffers(1, &airplaneBodyEBO);
    glBindVertexArray(airplaneBodyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, airplaneBodyVBO);
    glBufferData(GL_ARRAY_BUFFER, bodyVerts.size() * sizeof(float), bodyVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, airplaneBodyEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bodyInds.size() * sizeof(unsigned int), bodyInds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    
    // Main wings
    std::vector<float> wingVerts;
    std::vector<unsigned int> wingInds;
    
    float wingSpan = 6.0f, wingChord = 1.5f, wingThick = 0.1f;
    // Wing color - same as body with red stripe
    float wingR = 0.85f, wingG = 0.85f, wingB = 0.9f;
    
    // Top of wing
    wingVerts.insert(wingVerts.end(), {-wingSpan, wingThick, -wingChord * 0.3f, 0, 1, 0, wingR, wingG, wingB});
    wingVerts.insert(wingVerts.end(), {wingSpan, wingThick, -wingChord * 0.3f, 0, 1, 0, wingR, wingG, wingB});
    wingVerts.insert(wingVerts.end(), {wingSpan * 0.8f, wingThick, wingChord * 0.7f, 0, 1, 0, wingR, wingG, wingB});
    wingVerts.insert(wingVerts.end(), {-wingSpan * 0.8f, wingThick, wingChord * 0.7f, 0, 1, 0, wingR, wingG, wingB});
    wingInds.push_back(0); wingInds.push_back(1); wingInds.push_back(2);
    wingInds.push_back(0); wingInds.push_back(2); wingInds.push_back(3);
    
    // Bottom of wing
    wingVerts.insert(wingVerts.end(), {-wingSpan, -wingThick, -wingChord * 0.3f, 0, -1, 0, wingR, wingG, wingB});
    wingVerts.insert(wingVerts.end(), {wingSpan, -wingThick, -wingChord * 0.3f, 0, -1, 0, wingR, wingG, wingB});
    wingVerts.insert(wingVerts.end(), {wingSpan * 0.8f, -wingThick, wingChord * 0.7f, 0, -1, 0, wingR, wingG, wingB});
    wingVerts.insert(wingVerts.end(), {-wingSpan * 0.8f, -wingThick, wingChord * 0.7f, 0, -1, 0, wingR, wingG, wingB});
    wingInds.push_back(4); wingInds.push_back(6); wingInds.push_back(5);
    wingInds.push_back(4); wingInds.push_back(7); wingInds.push_back(6);
    
    airplaneWingIndexCount = (int)wingInds.size();
    
    glGenVertexArrays(1, &airplaneWingVAO);
    glGenBuffers(1, &airplaneWingVBO);
    glGenBuffers(1, &airplaneWingEBO);
    glBindVertexArray(airplaneWingVAO);
    glBindBuffer(GL_ARRAY_BUFFER, airplaneWingVBO);
    glBufferData(GL_ARRAY_BUFFER, wingVerts.size() * sizeof(float), wingVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, airplaneWingEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, wingInds.size() * sizeof(unsigned int), wingInds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    
    // Tail fin
    std::vector<float> tailVerts;
    std::vector<unsigned int> tailInds;
    
    float tailH = 1.5f, tailW = 0.1f, tailD = 1.0f;
    // Vertical stabilizer (tail fin)
    tailVerts.insert(tailVerts.end(), {0, 0, fl - tailD, -1, 0, 0, 0.8f, 0.1f, 0.1f});  // Red accent
    tailVerts.insert(tailVerts.end(), {0, tailH, fl, -1, 0, 0, 0.8f, 0.1f, 0.1f});
    tailVerts.insert(tailVerts.end(), {0, 0, fl, -1, 0, 0, 0.8f, 0.1f, 0.1f});
    tailInds.push_back(0); tailInds.push_back(1); tailInds.push_back(2);
    
    tailVerts.insert(tailVerts.end(), {tailW, 0, fl - tailD, 1, 0, 0, 0.8f, 0.1f, 0.1f});
    tailVerts.insert(tailVerts.end(), {tailW, 0, fl, 1, 0, 0, 0.8f, 0.1f, 0.1f});
    tailVerts.insert(tailVerts.end(), {tailW, tailH, fl, 1, 0, 0, 0.8f, 0.1f, 0.1f});
    tailInds.push_back(3); tailInds.push_back(4); tailInds.push_back(5);
    
    // Horizontal stabilizer
    float hTailSpan = 2.5f, hTailChord = 0.8f;
    unsigned int hStart = 6;
    tailVerts.insert(tailVerts.end(), {-hTailSpan, 0.1f, fl - hTailChord, 0, 1, 0, wingR, wingG, wingB});
    tailVerts.insert(tailVerts.end(), {hTailSpan, 0.1f, fl - hTailChord, 0, 1, 0, wingR, wingG, wingB});
    tailVerts.insert(tailVerts.end(), {hTailSpan * 0.7f, 0.1f, fl, 0, 1, 0, wingR, wingG, wingB});
    tailVerts.insert(tailVerts.end(), {-hTailSpan * 0.7f, 0.1f, fl, 0, 1, 0, wingR, wingG, wingB});
    tailInds.push_back(hStart); tailInds.push_back(hStart + 1); tailInds.push_back(hStart + 2);
    tailInds.push_back(hStart); tailInds.push_back(hStart + 2); tailInds.push_back(hStart + 3);
    
    airplaneTailIndexCount = (int)tailInds.size();
    
    glGenVertexArrays(1, &airplaneTailVAO);
    glGenBuffers(1, &airplaneTailVBO);
    glGenBuffers(1, &airplaneTailEBO);
    glBindVertexArray(airplaneTailVAO);
    glBindBuffer(GL_ARRAY_BUFFER, airplaneTailVBO);
    glBufferData(GL_ARRAY_BUFFER, tailVerts.size() * sizeof(float), tailVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, airplaneTailEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, tailInds.size() * sizeof(unsigned int), tailInds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
}

static void InitBirdsAndAirplane() {
    CreateBirdMeshes();
    CreateAirplaneMeshes();
    
    // Initialize birds - spawn at random positions around character
    birdBots.resize(NUM_BIRDS);
    for (int i = 0; i < NUM_BIRDS; i++) {
        float angle = (float)i / NUM_BIRDS * 6.28318f + (rand() / (float)RAND_MAX) * 0.5f;
        float dist = 30.0f + (rand() / (float)RAND_MAX) * 40.0f;
        float height = 10.0f + (rand() / (float)RAND_MAX) * 30.0f;
        
        birdBots[i].position = glm::vec3(
            cos(angle) * dist,
            height,
            sin(angle) * dist
        );
        
        // Random flight direction (circling with some variation)
        float flyAngle = angle + 1.57f + (rand() / (float)RAND_MAX - 0.5f) * 0.5f;
        float speed = 8.0f + (rand() / (float)RAND_MAX) * 6.0f;
        birdBots[i].velocity = glm::vec3(
            cos(flyAngle) * speed,
            (rand() / (float)RAND_MAX - 0.5f) * 2.0f,
            sin(flyAngle) * speed
        );
        
        birdBots[i].wingPhase = (rand() / (float)RAND_MAX) * 6.28318f;
        birdBots[i].wingSpeed = 8.0f + (rand() / (float)RAND_MAX) * 4.0f;
        birdBots[i].size = 0.8f + (rand() / (float)RAND_MAX) * 0.5f;
    }
    
    // Initialize airplane
    airplane.position = glm::vec3(-150.0f, 60.0f, -50.0f);
    airplane.direction = glm::normalize(glm::vec3(1.0f, 0.0f, 0.2f));
    airplane.timer = 0.0f;
    airplane.active = false;
}

static void UpdateBirdsAndAirplane(float dt) {
    // Only update in day world
    if (currentWorld != 0) return;
    
    // Update birds
    for (int i = 0; i < NUM_BIRDS; i++) {
        // Move bird
        birdBots[i].position += birdBots[i].velocity * dt;
        
        // Update wing animation
        birdBots[i].wingPhase += birdBots[i].wingSpeed * dt;
        
        // Keep birds circling around the scene
        glm::vec3 toCenter = -birdBots[i].position;
        toCenter.y = 0;
        float distFromCenter = glm::length(toCenter);
        
        // Gently steer back if too far
        if (distFromCenter > 80.0f) {
            glm::vec3 steering = glm::normalize(toCenter) * 2.0f * dt;
            birdBots[i].velocity.x += steering.x;
            birdBots[i].velocity.z += steering.z;
            // Renormalize speed
            float speed = glm::length(glm::vec2(birdBots[i].velocity.x, birdBots[i].velocity.z));
            float targetSpeed = 8.0f + (i % 3) * 3.0f;
            if (speed > 0.1f) {
                birdBots[i].velocity.x *= targetSpeed / speed;
                birdBots[i].velocity.z *= targetSpeed / speed;
            }
        }
        
        // Keep birds at reasonable height
        if (birdBots[i].position.y < 5.0f) birdBots[i].velocity.y += 5.0f * dt;
        if (birdBots[i].position.y > 50.0f) birdBots[i].velocity.y -= 5.0f * dt;
        birdBots[i].velocity.y *= 0.98f;  // Dampen vertical movement
    }
    
    // Update airplane - flies across every 20 seconds
    airplane.timer += dt;
    
    if (!airplane.active && airplane.timer >= 20.0f) {
        // Start new crossing
        airplane.active = true;
        airplane.timer = 0.0f;
        
        // Randomize starting position (high in sky, from one side)
        float side = (rand() % 2 == 0) ? -1.0f : 1.0f;
        airplane.position = glm::vec3(
            side * 200.0f,
            50.0f + (rand() / (float)RAND_MAX) * 40.0f,
            (rand() / (float)RAND_MAX - 0.5f) * 100.0f
        );
        airplane.direction = glm::normalize(glm::vec3(-side, 0.0f, (rand() / (float)RAND_MAX - 0.5f) * 0.3f));
    }
    
    if (airplane.active) {
        // Move airplane
        float planeSpeed = 60.0f;
        airplane.position += airplane.direction * planeSpeed * dt;
        
        // Deactivate when crossed to other side
        if (fabs(airplane.position.x) > 250.0f) {
            airplane.active = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Ring Warriors Initialization and Update (Night World Battle)
// ---------------------------------------------------------------------------

// Helper functions for building humanoid meshes (forward declarations if needed)
static void AddCylinderRW(std::vector<float>& vertices, std::vector<unsigned int>& indices,
                          float x, float y, float z, float radius, float height, 
                          float r, float g, float b, int segments = 8) {
    int baseIndex = vertices.size() / 9;
    
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * 6.28318f;
        float nx = cos(angle);
        float nz = sin(angle);
        
        vertices.push_back(x + nx * radius);
        vertices.push_back(y);
        vertices.push_back(z + nz * radius);
        vertices.push_back(nx); vertices.push_back(0.0f); vertices.push_back(nz);
        vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
        
        vertices.push_back(x + nx * radius);
        vertices.push_back(y + height);
        vertices.push_back(z + nz * radius);
        vertices.push_back(nx); vertices.push_back(0.0f); vertices.push_back(nz);
        vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
    }
    
    for (int i = 0; i < segments; i++) {
        int base = baseIndex + i * 2;
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }
}

static void AddSphereRW(std::vector<float>& vertices, std::vector<unsigned int>& indices,
                        float cx, float cy, float cz, float radius,
                        float r, float g, float b, int stacks = 8, int slices = 10) {
    int baseIndex = vertices.size() / 9;
    
    for (int i = 0; i <= stacks; i++) {
        float phi = 3.14159f * i / stacks;
        for (int j = 0; j <= slices; j++) {
            float theta = 6.28318f * j / slices;
            
            float x = sin(phi) * cos(theta);
            float y = cos(phi);
            float z = sin(phi) * sin(theta);
            
            vertices.push_back(cx + x * radius);
            vertices.push_back(cy + y * radius);
            vertices.push_back(cz + z * radius);
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
            vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
        }
    }
    
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            int a = baseIndex + i * (slices + 1) + j;
            int b = a + slices + 1;
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(a + 1);
            indices.push_back(b);
            indices.push_back(b + 1);
            indices.push_back(a + 1);
        }
    }
}

static void AddBoxRW(std::vector<float>& vertices, std::vector<unsigned int>& indices,
                     float cx, float cy, float cz, float w, float h, float d,
                     float r, float g, float b) {
    int baseIndex = vertices.size() / 9;
    float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;
    
    float corners[8][3] = {
        {cx - hw, cy - hh, cz - hd}, {cx + hw, cy - hh, cz - hd},
        {cx + hw, cy + hh, cz - hd}, {cx - hw, cy + hh, cz - hd},
        {cx - hw, cy - hh, cz + hd}, {cx + hw, cy - hh, cz + hd},
        {cx + hw, cy + hh, cz + hd}, {cx - hw, cy + hh, cz + hd}
    };
    
    int faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
        {1, 5, 6, 2}, {4, 5, 1, 0}, {3, 2, 6, 7}
    };
    float normals[6][3] = {
        {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}
    };
    
    for (int f = 0; f < 6; f++) {
        int faceBase = vertices.size() / 9;
        for (int v = 0; v < 4; v++) {
            int ci = faces[f][v];
            vertices.push_back(corners[ci][0]);
            vertices.push_back(corners[ci][1]);
            vertices.push_back(corners[ci][2]);
            vertices.push_back(normals[f][0]);
            vertices.push_back(normals[f][1]);
            vertices.push_back(normals[f][2]);
            vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
        }
        indices.push_back(faceBase);
        indices.push_back(faceBase + 1);
        indices.push_back(faceBase + 2);
        indices.push_back(faceBase);
        indices.push_back(faceBase + 2);
        indices.push_back(faceBase + 3);
    }
}

static void CreateWarriorMesh(GLuint& vao, GLuint& vbo, GLuint& ebo, int& indexCount, 
                              float r, float g, float b) {
    std::vector<float> verts;
    std::vector<unsigned int> inds;
    
    // Skin color
    float skinR = 0.9f, skinG = 0.75f, skinB = 0.6f;
    // Darker suit color
    float darkR = r * 0.3f, darkG = g * 0.3f, darkB = b * 0.3f;
    // White accents
    float whiteR = 0.95f, whiteG = 0.95f, whiteB = 0.95f;
    
    // HEAD (skin)
    AddSphereRW(verts, inds, 0.0f, 1.55f, 0.0f, 0.18f, skinR, skinG, skinB, 10, 12);
    
    // FACE FEATURES
    // Eyes (white with colored iris)
    AddSphereRW(verts, inds, -0.06f, 1.58f, -0.14f, 0.035f, 0.95f, 0.95f, 0.95f, 5, 6); // Left eye white
    AddSphereRW(verts, inds, 0.06f, 1.58f, -0.14f, 0.035f, 0.95f, 0.95f, 0.95f, 5, 6);  // Right eye white
    AddSphereRW(verts, inds, -0.06f, 1.58f, -0.16f, 0.02f, r, g, b, 4, 5);  // Left iris (suit color)
    AddSphereRW(verts, inds, 0.06f, 1.58f, -0.16f, 0.02f, r, g, b, 4, 5);   // Right iris
    AddSphereRW(verts, inds, -0.06f, 1.58f, -0.17f, 0.01f, 0.05f, 0.05f, 0.05f, 3, 4); // Left pupil
    AddSphereRW(verts, inds, 0.06f, 1.58f, -0.17f, 0.01f, 0.05f, 0.05f, 0.05f, 3, 4);  // Right pupil
    // Nose
    AddSphereRW(verts, inds, 0.0f, 1.52f, -0.15f, 0.025f, skinR * 0.95f, skinG * 0.95f, skinB * 0.95f, 4, 5);
    // Mouth (slight smile)
    AddBoxRW(verts, inds, 0.0f, 1.46f, -0.14f, 0.08f, 0.015f, 0.02f, 0.6f, 0.3f, 0.3f);
    // Eyebrows
    AddBoxRW(verts, inds, -0.06f, 1.63f, -0.14f, 0.05f, 0.015f, 0.02f, 0.3f, 0.2f, 0.15f);
    AddBoxRW(verts, inds, 0.06f, 1.63f, -0.14f, 0.05f, 0.015f, 0.02f, 0.3f, 0.2f, 0.15f);
    
    // MASK (covers top of head in suit color)
    AddSphereRW(verts, inds, 0.0f, 1.62f, 0.0f, 0.16f, r, g, b, 6, 8);
    
    // NECK
    AddCylinderRW(verts, inds, 0.0f, 1.28f, 0.0f, 0.08f, 0.12f, skinR, skinG, skinB);
    
    // TORSO - chest (main suit color)
    AddBoxRW(verts, inds, 0.0f, 1.0f, 0.0f, 0.48f, 0.5f, 0.26f, r, g, b);
    
    // RING EMBLEM on chest (white circle)
    AddCylinderRW(verts, inds, 0.0f, 1.05f, -0.14f, 0.1f, 0.02f, whiteR, whiteG, whiteB, 12);
    
    // WAIST/BELT (darker)
    AddBoxRW(verts, inds, 0.0f, 0.7f, 0.0f, 0.4f, 0.12f, 0.22f, darkR, darkG, darkB);
    
    // HIPS
    AddBoxRW(verts, inds, 0.0f, 0.55f, 0.0f, 0.42f, 0.18f, 0.24f, r, g, b);
    
    // SHOULDERS (joint spheres - main color)
    AddSphereRW(verts, inds, -0.3f, 1.18f, 0.0f, 0.1f, r, g, b, 6, 8);
    AddSphereRW(verts, inds, 0.3f, 1.18f, 0.0f, 0.1f, r, g, b, 6, 8);
    
    // UPPER ARMS (main suit color)
    AddCylinderRW(verts, inds, -0.35f, 0.88f, 0.0f, 0.075f, 0.3f, r, g, b);
    AddCylinderRW(verts, inds, 0.35f, 0.88f, 0.0f, 0.075f, 0.3f, r, g, b);
    
    // ELBOWS
    AddSphereRW(verts, inds, -0.35f, 0.88f, 0.0f, 0.065f, darkR, darkG, darkB, 5, 6);
    AddSphereRW(verts, inds, 0.35f, 0.88f, 0.0f, 0.065f, darkR, darkG, darkB, 5, 6);
    
    // FOREARMS (main suit color)
    AddCylinderRW(verts, inds, -0.35f, 0.58f, 0.0f, 0.06f, 0.3f, r, g, b);
    AddCylinderRW(verts, inds, 0.35f, 0.58f, 0.0f, 0.06f, 0.3f, r, g, b);
    
    // GLOVES/HANDS (white) - palm
    AddSphereRW(verts, inds, -0.35f, 0.55f, 0.0f, 0.055f, whiteR, whiteG, whiteB, 5, 6);
    AddSphereRW(verts, inds, 0.35f, 0.55f, 0.0f, 0.055f, whiteR, whiteG, whiteB, 5, 6);
    // FINGERS - Left hand
    AddCylinderRW(verts, inds, -0.38f, 0.48f, -0.02f, 0.012f, 0.06f, whiteR, whiteG, whiteB, 4); // thumb
    AddCylinderRW(verts, inds, -0.33f, 0.46f, -0.03f, 0.01f, 0.065f, whiteR, whiteG, whiteB, 4); // index
    AddCylinderRW(verts, inds, -0.35f, 0.45f, -0.01f, 0.01f, 0.07f, whiteR, whiteG, whiteB, 4);  // middle
    AddCylinderRW(verts, inds, -0.37f, 0.46f, 0.01f, 0.01f, 0.065f, whiteR, whiteG, whiteB, 4);  // ring
    AddCylinderRW(verts, inds, -0.39f, 0.47f, 0.02f, 0.009f, 0.05f, whiteR, whiteG, whiteB, 4);  // pinky
    // FINGERS - Right hand
    AddCylinderRW(verts, inds, 0.38f, 0.48f, -0.02f, 0.012f, 0.06f, whiteR, whiteG, whiteB, 4); // thumb
    AddCylinderRW(verts, inds, 0.33f, 0.46f, -0.03f, 0.01f, 0.065f, whiteR, whiteG, whiteB, 4); // index
    AddCylinderRW(verts, inds, 0.35f, 0.45f, -0.01f, 0.01f, 0.07f, whiteR, whiteG, whiteB, 4);  // middle
    AddCylinderRW(verts, inds, 0.37f, 0.46f, 0.01f, 0.01f, 0.065f, whiteR, whiteG, whiteB, 4);  // ring
    AddCylinderRW(verts, inds, 0.39f, 0.47f, 0.02f, 0.009f, 0.05f, whiteR, whiteG, whiteB, 4);  // pinky
    
    // RING on right hand (glowing same color as suit)
    AddCylinderRW(verts, inds, 0.35f, 0.52f, 0.0f, 0.04f, 0.03f, r * 1.5f, g * 1.5f, b * 1.5f, 8);
    
    // UPPER LEGS (main suit color)
    AddCylinderRW(verts, inds, -0.12f, 0.1f, 0.0f, 0.085f, 0.35f, r, g, b);
    AddCylinderRW(verts, inds, 0.12f, 0.1f, 0.0f, 0.085f, 0.35f, r, g, b);
    
    // KNEES (darker)
    AddSphereRW(verts, inds, -0.12f, 0.1f, 0.0f, 0.075f, darkR, darkG, darkB, 5, 6);
    AddSphereRW(verts, inds, 0.12f, 0.1f, 0.0f, 0.075f, darkR, darkG, darkB, 5, 6);
    
    // LOWER LEGS (main suit color)
    AddCylinderRW(verts, inds, -0.12f, -0.22f, 0.0f, 0.07f, 0.32f, r, g, b);
    AddCylinderRW(verts, inds, 0.12f, -0.22f, 0.0f, 0.07f, 0.32f, r, g, b);
    
    // BOOTS (darker)
    AddBoxRW(verts, inds, -0.12f, -0.32f, 0.02f, 0.11f, 0.14f, 0.18f, darkR, darkG, darkB);
    AddBoxRW(verts, inds, 0.12f, -0.32f, 0.02f, 0.11f, 0.14f, 0.18f, darkR, darkG, darkB);
    
    indexCount = (int)inds.size();
    
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
}

static void CreateColoredEnergyBall(GLuint& vao, GLuint& vbo, GLuint& ebo, int& indexCount,
                                    float r, float g, float b) {
    std::vector<float> verts;
    std::vector<unsigned int> inds;
    
    int stacks = 12;
    int slices = 16;
    float radius = 1.0f;
    
    for (int i = 0; i <= stacks; i++) {
        float phi = 3.14159f * i / stacks;
        for (int j = 0; j <= slices; j++) {
            float theta = 2.0f * 3.14159f * j / slices;
            
            float x = radius * sin(phi) * cos(theta);
            float y = radius * cos(phi);
            float z = radius * sin(phi) * sin(theta);
            
            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
            verts.push_back(r);
            verts.push_back(g);
            verts.push_back(b);
        }
    }
    
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            int first = i * (slices + 1) + j;
            int second = first + slices + 1;
            
            inds.push_back(first);
            inds.push_back(second);
            inds.push_back(first + 1);
            
            inds.push_back(second);
            inds.push_back(second + 1);
            inds.push_back(first + 1);
        }
    }
    
    indexCount = (int)inds.size();
    
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
}

static void CreateEnergyBallMesh() {
    // Create a sphere-like mesh for the energy ball
    std::vector<float> verts;
    std::vector<unsigned int> inds;
    
    int stacks = 12;
    int slices = 16;
    float radius = 1.0f;
    
    for (int i = 0; i <= stacks; i++) {
        float phi = 3.14159f * i / stacks;
        for (int j = 0; j <= slices; j++) {
            float theta = 2.0f * 3.14159f * j / slices;
            
            float x = radius * sin(phi) * cos(theta);
            float y = radius * cos(phi);
            float z = radius * sin(phi) * sin(theta);
            
            // Position
            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
            // Normal (same as position for sphere)
            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
            // Color (white, will be tinted by shader)
            verts.push_back(1.0f);
            verts.push_back(1.0f);
            verts.push_back(1.0f);
        }
    }
    
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            int first = i * (slices + 1) + j;
            int second = first + slices + 1;
            
            inds.push_back(first);
            inds.push_back(second);
            inds.push_back(first + 1);
            
            inds.push_back(second);
            inds.push_back(second + 1);
            inds.push_back(first + 1);
        }
    }
    
    energyBallIndexCount = (int)inds.size();
    
    glGenVertexArrays(1, &energyBallVAO);
    glGenBuffers(1, &energyBallVBO);
    glGenBuffers(1, &energyBallEBO);
    glBindVertexArray(energyBallVAO);
    glBindBuffer(GL_ARRAY_BUFFER, energyBallVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, energyBallEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    
    // Create colored versions
    int tempCount;
    CreateColoredEnergyBall(greenBallVAO, greenBallVBO, greenBallEBO, tempCount, 0.2f, 1.0f, 0.3f);  // Bright green
    CreateColoredEnergyBall(yellowBallVAO, yellowBallVBO, yellowBallEBO, tempCount, 1.0f, 0.9f, 0.1f);  // Bright yellow
    CreateColoredEnergyBall(mixedBallVAO, mixedBallVBO, mixedBallEBO, tempCount, 0.6f, 0.95f, 0.2f);  // Green-yellow mix (lime green)
}

static void CreateBeamMesh() {
    // Simple line mesh for beam
    float beamVerts[] = {
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };
    
    glGenVertexArrays(1, &beamVAO);
    glGenBuffers(1, &beamVBO);
    glBindVertexArray(beamVAO);
    glBindBuffer(GL_ARRAY_BUFFER, beamVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(beamVerts), beamVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

// Create an extended arm mesh pointing forward (for beam shooting poses)
static void CreateExtendedArmMesh(GLuint& vao, GLuint& vbo, GLuint& ebo, int& indexCount,
                                   float r, float g, float b, float skinR, float skinG, float skinB,
                                   float gloveR, float gloveG, float gloveB, bool hasRing) {
    std::vector<float> verts;
    std::vector<unsigned int> inds;
    
    // ARM pointing forward (-Z direction) - starts at shoulder position
    // Shoulder joint
    AddSphereRW(verts, inds, 0.0f, 0.0f, 0.0f, 0.1f, r, g, b, 6, 8);
    
    // Upper arm (pointing forward and slightly down)
    AddCylinderRW(verts, inds, 0.0f, -0.05f, -0.2f, 0.075f, 0.35f, r, g, b);
    
    // Elbow
    AddSphereRW(verts, inds, 0.0f, -0.1f, -0.4f, 0.065f, r * 0.5f, g * 0.5f, b * 0.5f, 5, 6);
    
    // Forearm
    AddCylinderRW(verts, inds, 0.0f, -0.12f, -0.6f, 0.06f, 0.35f, r, g, b);
    
    // Hand/fist (glove color)
    AddSphereRW(verts, inds, 0.0f, -0.12f, -0.82f, 0.07f, gloveR, gloveG, gloveB, 6, 8);
    
    // Fingers pointing forward (fist/open hand)
    AddCylinderRW(verts, inds, 0.0f, -0.1f, -0.9f, 0.02f, 0.08f, gloveR, gloveG, gloveB, 4);
    AddCylinderRW(verts, inds, 0.03f, -0.12f, -0.88f, 0.018f, 0.075f, gloveR, gloveG, gloveB, 4);
    AddCylinderRW(verts, inds, -0.03f, -0.12f, -0.88f, 0.018f, 0.075f, gloveR, gloveG, gloveB, 4);
    AddCylinderRW(verts, inds, 0.05f, -0.14f, -0.86f, 0.015f, 0.065f, gloveR, gloveG, gloveB, 4);
    AddCylinderRW(verts, inds, -0.05f, -0.14f, -0.86f, 0.015f, 0.065f, gloveR, gloveG, gloveB, 4);
    
    // Ring (if this is a ring warrior)
    if (hasRing) {
        AddCylinderRW(verts, inds, 0.0f, -0.12f, -0.85f, 0.045f, 0.025f, r * 1.5f, g * 1.5f, b * 1.5f, 8);
    }
    
    indexCount = (int)inds.size();
    
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
}

static void InitRingWarriors() {
    // Create meshes
    CreateWarriorMesh(heroBodyVAO, heroBodyVBO, heroBodyEBO, heroBodyIndexCount, 
                      0.0f, 0.9f, 0.2f);  // Bright Green
    CreateWarriorMesh(villainBodyVAO, villainBodyVBO, villainBodyEBO, villainBodyIndexCount,
                      1.0f, 0.85f, 0.0f);  // Bright Yellow
    CreateEnergyBallMesh();
    CreateBeamMesh();
    
    // Create extended arm meshes for beam shooting (single arm pointed forward)
    // Green Hero arm (green suit, white gloves, has ring)
    CreateExtendedArmMesh(heroArmVAO, heroArmVBO, heroArmEBO, heroArmIndexCount,
                          0.0f, 0.9f, 0.2f,   // suit color
                          0.9f, 0.75f, 0.6f,   // skin color
                          0.95f, 0.95f, 0.95f, // glove color
                          true);               // has ring
    // Yellow Villain arm (yellow suit, white gloves, has ring)
    CreateExtendedArmMesh(villainArmVAO, villainArmVBO, villainArmEBO, villainArmIndexCount,
                          1.0f, 0.85f, 0.0f,   // suit color
                          0.9f, 0.75f, 0.6f,   // skin color
                          0.95f, 0.95f, 0.95f, // glove color
                          true);               // has ring
    
    // Initialize positions - on the RIGHT side, FARTHER from player (more spaced out)
    // Green hero on one side, Yellow villain much farther on the other
    greenHero.homePosition = glm::vec3(35.0f, 25.0f, -20.0f);
    greenHero.position = greenHero.homePosition;
    greenHero.velocity = glm::vec3(0.0f);
    greenHero.bodyRotation = 1.57f;  // Facing right (toward villain)
    greenHero.armAngle = 0.0f;
    greenHero.isGreen = true;
    greenHero.shootPhase = 0.0f;
    
    yellowVillain.homePosition = glm::vec3(85.0f, 25.0f, -20.0f);  // Much farther apart (50 units gap)
    yellowVillain.position = yellowVillain.homePosition;
    yellowVillain.velocity = glm::vec3(0.0f);
    yellowVillain.bodyRotation = -1.57f;  // Facing left (toward hero)
    yellowVillain.armAngle = 0.0f;
    yellowVillain.isGreen = false;
    yellowVillain.shootPhase = 0.0f;
    
    // Initialize collision - midpoint between them
    energyCollision.position = glm::vec3(60.0f, 25.0f, -20.0f);
    energyCollision.radius = 0.5f;
    energyCollision.timer = 0.0f;
    energyCollision.maxTime = 10.0f;
    energyCollision.exploding = false;
    energyCollision.explosionTimer = 0.0f;
    energyCollision.shockwaveRadius = 0.0f;
    
    battleState = 0;
    returnTimer = 0.0f;
}

static void ResetRingWarriorsBattle() {
    // Set new home positions with some variation
    float baseZ = -20.0f + (rand() / (float)RAND_MAX - 0.5f) * 15.0f;
    float baseY = 25.0f + (rand() / (float)RAND_MAX - 0.5f) * 10.0f;
    
    greenHero.homePosition = glm::vec3(35.0f, baseY, baseZ);
    yellowVillain.homePosition = glm::vec3(85.0f, baseY, baseZ);
    
    // Reset collision
    energyCollision.position = (greenHero.homePosition + yellowVillain.homePosition) * 0.5f;
    energyCollision.radius = 0.5f;
    energyCollision.timer = 0.0f;
    energyCollision.exploding = false;
    energyCollision.explosionTimer = 0.0f;
    energyCollision.shockwaveRadius = 0.0f;
    
    beamParticles.clear();
    battleState = 0;
}

static void UpdateRingWarriors(float dt) {
    // Only update in night world
    if (currentWorld != 1) return;
    
    // Update shoot animation phase
    greenHero.shootPhase += dt * 3.0f;
    yellowVillain.shootPhase += dt * 3.0f;
    
    // State machine for battle
    if (battleState == 0) {
        // STATE 0: FIGHTING - charging up energy ball
        
        // Keep warriors at home position with bobbing
        float bobble = sin(greenHero.shootPhase) * 0.5f;
        greenHero.position.y = greenHero.homePosition.y + bobble;
        yellowVillain.position.y = yellowVillain.homePosition.y - bobble;
        greenHero.position.x = greenHero.homePosition.x;
        greenHero.position.z = greenHero.homePosition.z;
        yellowVillain.position.x = yellowVillain.homePosition.x;
        yellowVillain.position.z = yellowVillain.homePosition.z;
        
        // Make them face each other
        glm::vec3 toVillain = yellowVillain.position - greenHero.position;
        greenHero.bodyRotation = atan2(toVillain.x, -toVillain.z);
        yellowVillain.bodyRotation = atan2(-toVillain.x, toVillain.z);
        
        // Arms raised shooting pose
        greenHero.armAngle = 80.0f + sin(greenHero.shootPhase * 2.0f) * 5.0f;
        yellowVillain.armAngle = 80.0f + sin(yellowVillain.shootPhase * 2.0f) * 5.0f;
        
        // Building up energy ball
        energyCollision.timer += dt;
        
        // Ball grows over 10 seconds
        float progress = energyCollision.timer / energyCollision.maxTime;
        energyCollision.radius = 0.5f + progress * 10.0f;  // Grows from 0.5 to 10.5
        
        // Ball position stays in middle
        energyCollision.position = (greenHero.position + yellowVillain.position) * 0.5f;
        
        // Add beam particles flowing toward collision
        if (rand() % 3 == 0) {
            BeamParticle p;
            // From green hero
            p.position = greenHero.position + glm::vec3(1.0f, 0.3f, 0.0f);
            p.velocity = glm::normalize(energyCollision.position - p.position) * 20.0f;
            p.life = 1.0f;
            p.size = 0.3f + (rand() / (float)RAND_MAX) * 0.2f;
            p.isGreen = true;
            beamParticles.push_back(p);
            
            // From yellow villain
            p.position = yellowVillain.position + glm::vec3(-1.0f, 0.3f, 0.0f);
            p.velocity = glm::normalize(energyCollision.position - p.position) * 20.0f;
            p.isGreen = false;
            beamParticles.push_back(p);
        }
        
        // Play charging sound (console beep as placeholder)
        if (energyCollision.timer > 9.5f && energyCollision.timer < 9.6f) {
            std::cout << "\a" << std::flush;  // Warning beep before explosion
        }
        
        // Check if time to explode
        if (energyCollision.timer >= energyCollision.maxTime) {
            battleState = 1;  // Go to explosion state
            energyCollision.exploding = true;
            energyCollision.explosionTimer = 0.0f;
            energyCollision.shockwaveRadius = energyCollision.radius;
            
            // Explosion sound
            std::cout << "\a\a" << std::flush;  // Double beep for explosion
        }
        
    } else if (battleState == 1) {
        // STATE 1: EXPLOSION - warriors getting blown away
        
        energyCollision.explosionTimer += dt;
        
        // Shockwave expands rapidly
        energyCollision.shockwaveRadius += dt * 60.0f;
        
        // Push warriors away from explosion
        glm::vec3 heroFromCenter = greenHero.position - energyCollision.position;
        glm::vec3 villainFromCenter = yellowVillain.position - energyCollision.position;
        
        float heroDist = glm::length(heroFromCenter);
        float villainDist = glm::length(villainFromCenter);
        
        if (heroDist > 0.1f) {
            glm::vec3 pushDir = glm::normalize(heroFromCenter);
            greenHero.position += pushDir * dt * 100.0f;
        }
        if (villainDist > 0.1f) {
            glm::vec3 pushDir = glm::normalize(villainFromCenter);
            yellowVillain.position += pushDir * dt * 100.0f;
        }
        
        // Ball shrinks during explosion
        energyCollision.radius = glm::max(0.0f, energyCollision.radius - dt * 20.0f);
        
        // Add explosion particles
        for (int i = 0; i < 5; i++) {
            BeamParticle p;
            float angle = (rand() / (float)RAND_MAX) * 6.28318f;
            float upAngle = (rand() / (float)RAND_MAX) * 3.14159f;
            p.position = energyCollision.position;
            p.velocity = glm::vec3(
                sin(upAngle) * cos(angle) * 25.0f,
                cos(upAngle) * 25.0f,
                sin(upAngle) * sin(angle) * 25.0f
            );
            p.life = 1.0f;
            p.size = 0.4f + (rand() / (float)RAND_MAX) * 0.3f;
            p.isGreen = (rand() % 2 == 0);
            beamParticles.push_back(p);
        }
        
        // After explosion finishes, go to return state
        if (energyCollision.explosionTimer > 2.5f) {
            battleState = 2;  // Go to return state
            returnTimer = 0.0f;
            energyCollision.exploding = false;
        }
        
    } else if (battleState == 2) {
        // STATE 2: RETURNING - warriors fly back to home positions
        
        returnTimer += dt;
        
        // Smoothly interpolate back to home position
        float returnSpeed = 3.0f;  // How fast they return
        greenHero.position = glm::mix(greenHero.position, greenHero.homePosition, dt * returnSpeed);
        yellowVillain.position = glm::mix(yellowVillain.position, yellowVillain.homePosition, dt * returnSpeed);
        
        // Make them face each other while returning
        glm::vec3 toVillain = yellowVillain.position - greenHero.position;
        greenHero.bodyRotation = atan2(toVillain.x, -toVillain.z);
        yellowVillain.bodyRotation = atan2(-toVillain.x, toVillain.z);
        
        // Arms down while returning
        greenHero.armAngle = glm::mix(greenHero.armAngle, 0.0f, dt * 3.0f);
        yellowVillain.armAngle = glm::mix(yellowVillain.armAngle, 0.0f, dt * 3.0f);
        
        // Check if both warriors are close enough to home
        float heroDist = glm::length(greenHero.position - greenHero.homePosition);
        float villainDist = glm::length(yellowVillain.position - yellowVillain.homePosition);
        
        // After returning (or 3 seconds max), start new battle
        if ((heroDist < 1.0f && villainDist < 1.0f) || returnTimer > 3.0f) {
            ResetRingWarriorsBattle();
        }
    }
    
    // Update particles
    for (size_t i = 0; i < beamParticles.size(); ) {
        beamParticles[i].position += beamParticles[i].velocity * dt;
        beamParticles[i].life -= dt * 0.5f;
        
        if (beamParticles[i].life <= 0.0f) {
            beamParticles.erase(beamParticles.begin() + i);
        } else {
            i++;
        }
    }
    
    // Limit particles
    while (beamParticles.size() > 500) {
        beamParticles.erase(beamParticles.begin());
    }
}

// ---------------------------------------------------------------------------
// Flash Racing System (speedforce world)
// ---------------------------------------------------------------------------

// Reuse helper functions for speedster humanoids
static void CreateSpeedsterMesh(GLuint& vao, GLuint& vbo, GLuint& ebo, int& indexCount,
                                float r, float g, float b) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    // Determine if Flash (red) or Reverse Flash (yellow)
    bool isFlash = (r > g);  // Flash is red, Reverse is yellow
    
    // Skin color
    float skinR = 0.92f, skinG = 0.75f, skinB = 0.62f;
    // Darker accent color
    float darkR = r * 0.4f, darkG = g * 0.4f, darkB = b * 0.4f;
    // Lightning emblem color (yellow for Flash, red for Reverse)
    float embR = isFlash ? 1.0f : 0.9f;
    float embG = isFlash ? 0.9f : 0.2f;
    float embB = isFlash ? 0.2f : 0.1f;
    
    // HEAD with cowl/mask (main color)
    AddSphereRW(vertices, indices, 0.0f, 1.65f, 0.0f, 0.17f, r, g, b, 10, 12);
    
    // Face opening (skin showing through mask)
    AddSphereRW(vertices, indices, 0.0f, 1.62f, -0.08f, 0.1f, skinR, skinG, skinB, 6, 8);
    
    // FACE FEATURES (visible through mask opening)
    // Eyes (white with colored iris matching suit)
    AddSphereRW(vertices, indices, -0.04f, 1.64f, -0.13f, 0.025f, 0.95f, 0.95f, 0.95f, 4, 5); // Left eye white
    AddSphereRW(vertices, indices, 0.04f, 1.64f, -0.13f, 0.025f, 0.95f, 0.95f, 0.95f, 4, 5);  // Right eye white
    AddSphereRW(vertices, indices, -0.04f, 1.64f, -0.145f, 0.015f, embR, embG, embB, 3, 4);  // Left iris (emblem color)
    AddSphereRW(vertices, indices, 0.04f, 1.64f, -0.145f, 0.015f, embR, embG, embB, 3, 4);   // Right iris
    AddSphereRW(vertices, indices, -0.04f, 1.64f, -0.15f, 0.008f, 0.05f, 0.05f, 0.05f, 3, 4); // Left pupil
    AddSphereRW(vertices, indices, 0.04f, 1.64f, -0.15f, 0.008f, 0.05f, 0.05f, 0.05f, 3, 4);  // Right pupil
    // Nose
    AddSphereRW(vertices, indices, 0.0f, 1.60f, -0.12f, 0.018f, skinR * 0.95f, skinG * 0.95f, skinB * 0.95f, 4, 4);
    // Mouth (determined grin)
    AddBoxRW(vertices, indices, 0.0f, 1.56f, -0.11f, 0.05f, 0.01f, 0.015f, 0.5f, 0.25f, 0.25f);
    // Chin
    AddSphereRW(vertices, indices, 0.0f, 1.54f, -0.1f, 0.025f, skinR, skinG, skinB, 4, 5);
    
    // Cowl ear wings (signature Flash look)
    if (isFlash) {
        // Flash has small wing ears
        AddCylinderRW(vertices, indices, -0.14f, 1.72f, 0.0f, 0.02f, 0.08f, embR, embG, embB, 4);
        AddCylinderRW(vertices, indices, 0.14f, 1.72f, 0.0f, 0.02f, 0.08f, embR, embG, embB, 4);
    } else {
        // Reverse Flash has pointed ears
        AddCylinderRW(vertices, indices, -0.12f, 1.74f, 0.0f, 0.025f, 0.1f, darkR, darkG, darkB, 4);
        AddCylinderRW(vertices, indices, 0.12f, 1.74f, 0.0f, 0.025f, 0.1f, darkR, darkG, darkB, 4);
    }
    
    // NECK
    AddCylinderRW(vertices, indices, 0.0f, 1.38f, 0.0f, 0.07f, 0.12f, r, g, b);
    
    // TORSO - chest (main suit color)
    AddBoxRW(vertices, indices, 0.0f, 1.08f, 0.0f, 0.44f, 0.48f, 0.24f, r, g, b);
    
    // Lightning bolt emblem on chest
    AddBoxRW(vertices, indices, 0.0f, 1.1f, -0.13f, 0.12f, 0.2f, 0.02f, embR, embG, embB);
    AddBoxRW(vertices, indices, -0.03f, 1.0f, -0.13f, 0.08f, 0.12f, 0.02f, embR, embG, embB);
    
    // WAIST (darker belt area)
    AddBoxRW(vertices, indices, 0.0f, 0.78f, 0.0f, 0.38f, 0.12f, 0.2f, darkR, darkG, darkB);
    // Belt buckle (lightning bolt colored)
    AddBoxRW(vertices, indices, 0.0f, 0.78f, -0.11f, 0.08f, 0.06f, 0.02f, embR, embG, embB);
    
    // HIPS
    AddBoxRW(vertices, indices, 0.0f, 0.62f, 0.0f, 0.4f, 0.2f, 0.22f, r, g, b);
    
    // SHOULDERS
    AddSphereRW(vertices, indices, -0.28f, 1.22f, 0.0f, 0.09f, r, g, b, 6, 8);
    AddSphereRW(vertices, indices, 0.28f, 1.22f, 0.0f, 0.09f, r, g, b, 6, 8);
    
    // UPPER ARMS
    AddCylinderRW(vertices, indices, -0.32f, 0.94f, 0.0f, 0.065f, 0.28f, r, g, b);
    AddCylinderRW(vertices, indices, 0.32f, 0.94f, 0.0f, 0.065f, 0.28f, r, g, b);
    
    // ELBOWS
    AddSphereRW(vertices, indices, -0.32f, 0.94f, 0.0f, 0.055f, darkR, darkG, darkB, 5, 6);
    AddSphereRW(vertices, indices, 0.32f, 0.94f, 0.0f, 0.055f, darkR, darkG, darkB, 5, 6);
    
    // FOREARMS
    AddCylinderRW(vertices, indices, -0.32f, 0.66f, 0.0f, 0.055f, 0.28f, r, g, b);
    AddCylinderRW(vertices, indices, 0.32f, 0.66f, 0.0f, 0.055f, 0.28f, r, g, b);
    
    // GLOVES (darker) - palm
    AddSphereRW(vertices, indices, -0.32f, 0.62f, 0.0f, 0.045f, darkR, darkG, darkB, 5, 6);
    AddSphereRW(vertices, indices, 0.32f, 0.62f, 0.0f, 0.045f, darkR, darkG, darkB, 5, 6);
    // FINGERS - Left hand
    AddCylinderRW(vertices, indices, -0.35f, 0.56f, -0.015f, 0.01f, 0.05f, darkR, darkG, darkB, 4); // thumb
    AddCylinderRW(vertices, indices, -0.30f, 0.54f, -0.02f, 0.008f, 0.055f, darkR, darkG, darkB, 4); // index
    AddCylinderRW(vertices, indices, -0.32f, 0.53f, -0.005f, 0.008f, 0.06f, darkR, darkG, darkB, 4);  // middle
    AddCylinderRW(vertices, indices, -0.34f, 0.54f, 0.01f, 0.008f, 0.055f, darkR, darkG, darkB, 4);  // ring
    AddCylinderRW(vertices, indices, -0.36f, 0.55f, 0.02f, 0.007f, 0.045f, darkR, darkG, darkB, 4);  // pinky
    // FINGERS - Right hand
    AddCylinderRW(vertices, indices, 0.35f, 0.56f, -0.015f, 0.01f, 0.05f, darkR, darkG, darkB, 4); // thumb
    AddCylinderRW(vertices, indices, 0.30f, 0.54f, -0.02f, 0.008f, 0.055f, darkR, darkG, darkB, 4); // index
    AddCylinderRW(vertices, indices, 0.32f, 0.53f, -0.005f, 0.008f, 0.06f, darkR, darkG, darkB, 4);  // middle
    AddCylinderRW(vertices, indices, 0.34f, 0.54f, 0.01f, 0.008f, 0.055f, darkR, darkG, darkB, 4);  // ring
    AddCylinderRW(vertices, indices, 0.36f, 0.55f, 0.02f, 0.007f, 0.045f, darkR, darkG, darkB, 4);  // pinky
    // Lightning bolt on gloves
    AddCylinderRW(vertices, indices, -0.32f, 0.64f, -0.04f, 0.015f, 0.04f, embR, embG, embB, 4);
    AddCylinderRW(vertices, indices, 0.32f, 0.64f, -0.04f, 0.015f, 0.04f, embR, embG, embB, 4);
    
    // UPPER LEGS
    AddCylinderRW(vertices, indices, -0.1f, 0.18f, 0.0f, 0.08f, 0.34f, r, g, b);
    AddCylinderRW(vertices, indices, 0.1f, 0.18f, 0.0f, 0.08f, 0.34f, r, g, b);
    
    // KNEES
    AddSphereRW(vertices, indices, -0.1f, 0.18f, 0.0f, 0.07f, darkR, darkG, darkB, 5, 6);
    AddSphereRW(vertices, indices, 0.1f, 0.18f, 0.0f, 0.07f, darkR, darkG, darkB, 5, 6);
    
    // LOWER LEGS  
    AddCylinderRW(vertices, indices, -0.1f, -0.14f, 0.0f, 0.065f, 0.32f, r, g, b);
    AddCylinderRW(vertices, indices, 0.1f, -0.14f, 0.0f, 0.065f, 0.32f, r, g, b);
    
    // BOOTS (darker with lightning accents)
    AddBoxRW(vertices, indices, -0.1f, -0.26f, 0.02f, 0.1f, 0.16f, 0.16f, darkR, darkG, darkB);
    AddBoxRW(vertices, indices, 0.1f, -0.26f, 0.02f, 0.1f, 0.16f, 0.16f, darkR, darkG, darkB);
    // Boot lightning
    AddCylinderRW(vertices, indices, -0.1f, -0.22f, -0.06f, 0.015f, 0.08f, embR, embG, embB, 4);
    AddCylinderRW(vertices, indices, 0.1f, -0.22f, -0.06f, 0.015f, 0.08f, embR, embG, embB, 4);
    
    indexCount = indices.size();
    
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

static void CreateLightningBoltMesh() {
    // Create a small lightning bolt for trail effects
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    // Jagged lightning bolt shape
    float points[][3] = {
        {0.0f, 0.5f, 0.0f},
        {-0.15f, 0.25f, 0.0f},
        {0.05f, 0.2f, 0.0f},
        {-0.1f, 0.0f, 0.0f},
        {0.1f, -0.05f, 0.0f},
        {0.0f, -0.5f, 0.0f}
    };
    
    // White/bright lightning
    for (int i = 0; i < 6; i++) {
        vertices.push_back(points[i][0]);
        vertices.push_back(points[i][1]);
        vertices.push_back(points[i][2]);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
        vertices.push_back(1.0f);
        vertices.push_back(1.0f);
        vertices.push_back(0.8f);
    }
    
    // Create triangles
    indices.push_back(0); indices.push_back(1); indices.push_back(2);
    indices.push_back(1); indices.push_back(3); indices.push_back(2);
    indices.push_back(2); indices.push_back(3); indices.push_back(4);
    indices.push_back(3); indices.push_back(5); indices.push_back(4);
    
    flashLightningIndexCount = indices.size();
    
    glGenVertexArrays(1, &flashLightningVAO);
    glGenBuffers(1, &flashLightningVBO);
    glGenBuffers(1, &flashLightningEBO);
    
    glBindVertexArray(flashLightningVAO);
    glBindBuffer(GL_ARRAY_BUFFER, flashLightningVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, flashLightningEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

static void InitSpeedsters() {
    // Create Flash (red costume with yellow lightning)
    CreateSpeedsterMesh(flashBodyVAO, flashBodyVBO, flashBodyEBO, flashBodyIndexCount,
                        0.9f, 0.1f, 0.1f);  // Bright red
    
    // Create Reverse Flash (yellow costume with red lightning)  
    CreateSpeedsterMesh(reverseFlashBodyVAO, reverseFlashBodyVBO, reverseFlashBodyEBO, 
                        reverseFlashBodyIndexCount, 1.0f, 0.8f, 0.0f);  // Yellow
    
    CreateLightningBoltMesh();
    
    // Initialize Flash (inner lane)
    theFlash.position = glm::vec3(raceTrackRadius, 0.0f, 0.0f);
    theFlash.velocity = glm::vec3(0.0f);
    theFlash.bodyRotation = 0.0f;
    theFlash.runPhase = 0.0f;
    theFlash.armSwing = 0.0f;
    theFlash.legSwing = 0.0f;
    theFlash.isFlash = true;
    theFlash.laneOffset = -3.0f;  // Inner lane
    theFlash.speedBoost = 1.0f;
    theFlash.lap = 0;
    
    // Initialize Reverse Flash (outer lane)
    reverseFlash.position = glm::vec3(raceTrackRadius + 6.0f, 0.0f, 0.0f);
    reverseFlash.velocity = glm::vec3(0.0f);
    reverseFlash.bodyRotation = 0.0f;
    reverseFlash.runPhase = 0.5f;  // Offset phase
    reverseFlash.armSwing = 0.0f;
    reverseFlash.legSwing = 0.0f;
    reverseFlash.isFlash = false;
    reverseFlash.laneOffset = 3.0f;  // Outer lane
    reverseFlash.speedBoost = 1.0f;
    reverseFlash.lap = 0;
    
    flashRaceTimer = 0.0f;
    flashLeader = 0;
}

static void UpdateSpeedsters(float dt) {
    // Only update in speedforce world
    if (currentWorld != 3) return;
    
    flashRaceTimer += dt;
    
    // Racing speed (very fast - it's the speedforce!)
    float baseSpeed = 180.0f;  // degrees per second around the track
    
    // Add some variation - they trade leads
    float flashSpeedMod = 1.0f + sin(flashRaceTimer * 0.5f) * 0.15f;
    float reverseSpeedMod = 1.0f + sin(flashRaceTimer * 0.5f + 1.57f) * 0.15f;
    
    // Occasional speed bursts
    if (sin(flashRaceTimer * 2.0f) > 0.9f) {
        flashSpeedMod += 0.3f;
    }
    if (sin(flashRaceTimer * 2.0f + 3.14f) > 0.9f) {
        reverseSpeedMod += 0.3f;
    }
    
    // Update run phase (animation speed)
    theFlash.runPhase += dt * baseSpeed * flashSpeedMod * 0.1f;
    reverseFlash.runPhase += dt * baseSpeed * reverseSpeedMod * 0.1f;
    
    // Calculate angular positions on circular track
    float flashAngle = theFlash.runPhase * 0.1f;  // Convert to radians
    float reverseAngle = reverseFlash.runPhase * 0.1f;
    
    // Inner and outer track radii
    float flashRadius = raceTrackRadius + theFlash.laneOffset;
    float reverseRadius = raceTrackRadius + reverseFlash.laneOffset;
    
    // Update positions (circular track around the player)
    theFlash.position.x = cos(flashAngle) * flashRadius;
    theFlash.position.z = sin(flashAngle) * flashRadius;
    theFlash.position.y = 5.0f + sin(theFlash.runPhase * 0.5f) * 0.3f;  // Slight bob
    
    reverseFlash.position.x = cos(reverseAngle) * reverseRadius;
    reverseFlash.position.z = sin(reverseAngle) * reverseRadius;
    reverseFlash.position.y = 5.0f + sin(reverseFlash.runPhase * 0.5f + 1.0f) * 0.3f;
    
    // Face direction of movement (tangent to circle)
    theFlash.bodyRotation = flashAngle + 1.5708f;  // 90 degrees ahead
    reverseFlash.bodyRotation = reverseAngle + 1.5708f;
    
    // Arm and leg swing animation
    theFlash.armSwing = sin(theFlash.runPhase * 3.0f) * 60.0f;
    theFlash.legSwing = sin(theFlash.runPhase * 3.0f) * 45.0f;
    
    reverseFlash.armSwing = sin(reverseFlash.runPhase * 3.0f) * 60.0f;
    reverseFlash.legSwing = sin(reverseFlash.runPhase * 3.0f) * 45.0f;
    
    // Determine who's leading
    if (flashAngle > reverseAngle + 0.1f) {
        flashLeader = 0;
    } else if (reverseAngle > flashAngle + 0.1f) {
        flashLeader = 1;
    }
    
    // Add lightning trail particles
    if (rand() % 2 == 0) {
        LightningTrail trail;
        // Flash trail (red/yellow)
        trail.position = theFlash.position;
        trail.position.x += (rand() / (float)RAND_MAX - 0.5f) * 2.0f;
        trail.position.y += (rand() / (float)RAND_MAX) * 1.5f;
        trail.position.z += (rand() / (float)RAND_MAX - 0.5f) * 2.0f;
        trail.velocity = glm::vec3(
            (rand() / (float)RAND_MAX - 0.5f) * 5.0f,
            (rand() / (float)RAND_MAX) * 3.0f,
            (rand() / (float)RAND_MAX - 0.5f) * 5.0f
        );
        trail.life = 0.8f;
        trail.size = 0.3f + (rand() / (float)RAND_MAX) * 0.3f;
        trail.isRed = true;
        lightningTrails.push_back(trail);
        
        // Reverse Flash trail (yellow/red)
        trail.position = reverseFlash.position;
        trail.position.x += (rand() / (float)RAND_MAX - 0.5f) * 2.0f;
        trail.position.y += (rand() / (float)RAND_MAX) * 1.5f;
        trail.position.z += (rand() / (float)RAND_MAX - 0.5f) * 2.0f;
        trail.velocity = glm::vec3(
            (rand() / (float)RAND_MAX - 0.5f) * 5.0f,
            (rand() / (float)RAND_MAX) * 3.0f,
            (rand() / (float)RAND_MAX - 0.5f) * 5.0f
        );
        trail.isRed = false;
        lightningTrails.push_back(trail);
    }
    
    // Update lightning trails
    for (size_t i = 0; i < lightningTrails.size(); ) {
        lightningTrails[i].position += lightningTrails[i].velocity * dt;
        lightningTrails[i].life -= dt * 1.5f;
        
        if (lightningTrails[i].life <= 0.0f) {
            lightningTrails.erase(lightningTrails.begin() + i);
        } else {
            i++;
        }
    }
    
    // Limit trail particles
    while (lightningTrails.size() > 300) {
        lightningTrails.erase(lightningTrails.begin());
    }
}

// ---------------------------------------------------------------------------
// DBZ Goku vs Cell System (chrome world)
// ---------------------------------------------------------------------------
// Uses the existing AddCylinderRW, AddSphereRW, AddBoxRW helper functions

static void CreateDBZFighterMesh(GLuint& vao, GLuint& vbo, GLuint& ebo, int& indexCount,
                                  float r, float g, float b, bool isGoku) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    if (isGoku) {
        // ============ GOKU - Super Saiyan ============
        // Skin color
        float skinR = 0.95f, skinG = 0.75f, skinB = 0.6f;
        // Orange gi
        float giR = 1.0f, giG = 0.45f, giB = 0.1f;
        // Blue undershirt
        float blueR = 0.2f, blueG = 0.3f, blueB = 0.8f;
        // Golden hair
        float hairR = 1.0f, hairG = 0.85f, hairB = 0.2f;
        
        // HEAD (skin)
        AddSphereRW(vertices, indices, 0.0f, 1.65f, 0.0f, 0.18f, skinR, skinG, skinB, 10, 14);
        
        // GOKU FACE FEATURES
        // Eyes (determined warrior look)
        AddSphereRW(vertices, indices, -0.055f, 1.68f, -0.14f, 0.035f, 0.98f, 0.98f, 0.98f, 5, 6); // Left eye white
        AddSphereRW(vertices, indices, 0.055f, 1.68f, -0.14f, 0.035f, 0.98f, 0.98f, 0.98f, 5, 6);  // Right eye white
        // Super Saiyan green/teal eyes
        AddSphereRW(vertices, indices, -0.055f, 1.68f, -0.16f, 0.02f, 0.3f, 0.9f, 0.7f, 4, 5);  // Left iris
        AddSphereRW(vertices, indices, 0.055f, 1.68f, -0.16f, 0.02f, 0.3f, 0.9f, 0.7f, 4, 5);   // Right iris
        AddSphereRW(vertices, indices, -0.055f, 1.68f, -0.17f, 0.01f, 0.05f, 0.05f, 0.05f, 3, 4); // Left pupil
        AddSphereRW(vertices, indices, 0.055f, 1.68f, -0.17f, 0.01f, 0.05f, 0.05f, 0.05f, 3, 4);  // Right pupil
        // Eyebrows (intense/angled for Super Saiyan)
        AddBoxRW(vertices, indices, -0.06f, 1.74f, -0.14f, 0.055f, 0.018f, 0.02f, hairR, hairG, hairB);
        AddBoxRW(vertices, indices, 0.06f, 1.74f, -0.14f, 0.055f, 0.018f, 0.02f, hairR, hairG, hairB);
        // Nose
        AddSphereRW(vertices, indices, 0.0f, 1.62f, -0.15f, 0.025f, skinR * 0.95f, skinG * 0.95f, skinB * 0.95f, 4, 5);
        // Mouth (determined expression)
        AddBoxRW(vertices, indices, 0.0f, 1.56f, -0.14f, 0.06f, 0.015f, 0.02f, 0.65f, 0.35f, 0.35f);
        // Ears
        AddSphereRW(vertices, indices, -0.16f, 1.64f, 0.0f, 0.035f, skinR, skinG, skinB, 4, 5);
        AddSphereRW(vertices, indices, 0.16f, 1.64f, 0.0f, 0.035f, skinR, skinG, skinB, 4, 5);
        
        // NECK
        AddCylinderRW(vertices, indices, 0.0f, 1.35f, 0.0f, 0.08f, 0.15f, skinR, skinG, skinB);
        
        // TORSO - chest (orange gi)
        AddBoxRW(vertices, indices, 0.0f, 1.05f, 0.0f, 0.5f, 0.55f, 0.28f, giR, giG, giB);
        
        // TORSO - abdomen (blue undershirt shows)
        AddBoxRW(vertices, indices, 0.0f, 0.7f, 0.0f, 0.4f, 0.2f, 0.22f, blueR, blueG, blueB);
        
        // WAIST/HIPS (orange gi pants)
        AddBoxRW(vertices, indices, 0.0f, 0.5f, 0.0f, 0.42f, 0.2f, 0.24f, giR, giG, giB);
        
        // SHOULDERS (joint spheres)
        AddSphereRW(vertices, indices, -0.32f, 1.25f, 0.0f, 0.1f, giR, giG, giB, 6, 8);
        AddSphereRW(vertices, indices, 0.32f, 1.25f, 0.0f, 0.1f, giR, giG, giB, 6, 8);
        
        // UPPER ARMS (orange gi sleeves)
        AddCylinderRW(vertices, indices, -0.38f, 0.95f, 0.0f, 0.08f, 0.3f, giR, giG, giB);
        AddCylinderRW(vertices, indices, 0.38f, 0.95f, 0.0f, 0.08f, 0.3f, giR, giG, giB);
        
        // ELBOWS
        AddSphereRW(vertices, indices, -0.38f, 0.95f, 0.0f, 0.07f, skinR, skinG, skinB, 5, 6);
        AddSphereRW(vertices, indices, 0.38f, 0.95f, 0.0f, 0.07f, skinR, skinG, skinB, 5, 6);
        
        // FOREARMS (skin - sleeveless)
        AddCylinderRW(vertices, indices, -0.38f, 0.65f, 0.0f, 0.065f, 0.3f, skinR, skinG, skinB);
        AddCylinderRW(vertices, indices, 0.38f, 0.65f, 0.0f, 0.065f, 0.3f, skinR, skinG, skinB);
        
        // HANDS (skin) - palm
        AddSphereRW(vertices, indices, -0.38f, 0.62f, 0.0f, 0.055f, skinR, skinG, skinB, 5, 6);
        AddSphereRW(vertices, indices, 0.38f, 0.62f, 0.0f, 0.055f, skinR, skinG, skinB, 5, 6);
        // GOKU FINGERS - Left hand
        AddCylinderRW(vertices, indices, -0.41f, 0.55f, -0.015f, 0.012f, 0.055f, skinR, skinG, skinB, 4); // thumb
        AddCylinderRW(vertices, indices, -0.36f, 0.53f, -0.025f, 0.01f, 0.06f, skinR, skinG, skinB, 4); // index
        AddCylinderRW(vertices, indices, -0.38f, 0.52f, -0.01f, 0.01f, 0.065f, skinR, skinG, skinB, 4);  // middle
        AddCylinderRW(vertices, indices, -0.40f, 0.53f, 0.008f, 0.01f, 0.06f, skinR, skinG, skinB, 4);  // ring
        AddCylinderRW(vertices, indices, -0.42f, 0.54f, 0.02f, 0.009f, 0.05f, skinR, skinG, skinB, 4);  // pinky
        // GOKU FINGERS - Right hand
        AddCylinderRW(vertices, indices, 0.41f, 0.55f, -0.015f, 0.012f, 0.055f, skinR, skinG, skinB, 4); // thumb
        AddCylinderRW(vertices, indices, 0.36f, 0.53f, -0.025f, 0.01f, 0.06f, skinR, skinG, skinB, 4); // index
        AddCylinderRW(vertices, indices, 0.38f, 0.52f, -0.01f, 0.01f, 0.065f, skinR, skinG, skinB, 4);  // middle
        AddCylinderRW(vertices, indices, 0.40f, 0.53f, 0.008f, 0.01f, 0.06f, skinR, skinG, skinB, 4);  // ring
        AddCylinderRW(vertices, indices, 0.42f, 0.54f, 0.02f, 0.009f, 0.05f, skinR, skinG, skinB, 4);  // pinky
        
        // WRISTBANDS (blue)
        AddCylinderRW(vertices, indices, -0.38f, 0.68f, 0.0f, 0.072f, 0.06f, blueR, blueG, blueB);
        AddCylinderRW(vertices, indices, 0.38f, 0.68f, 0.0f, 0.072f, 0.06f, blueR, blueG, blueB);
        
        // UPPER LEGS (orange gi pants)
        AddCylinderRW(vertices, indices, -0.12f, 0.05f, 0.0f, 0.09f, 0.35f, giR, giG, giB);
        AddCylinderRW(vertices, indices, 0.12f, 0.05f, 0.0f, 0.09f, 0.35f, giR, giG, giB);
        
        // KNEES
        AddSphereRW(vertices, indices, -0.12f, 0.05f, 0.0f, 0.08f, giR, giG, giB, 5, 6);
        AddSphereRW(vertices, indices, 0.12f, 0.05f, 0.0f, 0.08f, giR, giG, giB, 5, 6);
        
        // LOWER LEGS (orange gi)
        AddCylinderRW(vertices, indices, -0.12f, -0.28f, 0.0f, 0.075f, 0.33f, giR, giG, giB);
        AddCylinderRW(vertices, indices, 0.12f, -0.28f, 0.0f, 0.075f, 0.33f, giR, giG, giB);
        
        // BOOTS (blue)
        AddBoxRW(vertices, indices, -0.12f, -0.38f, 0.02f, 0.12f, 0.15f, 0.2f, blueR, blueG, blueB);
        AddBoxRW(vertices, indices, 0.12f, -0.38f, 0.02f, 0.12f, 0.15f, 0.2f, blueR, blueG, blueB);
        
        // SUPER SAIYAN HAIR - Multiple golden spikes
        // Main upward spikes
        for (int i = 0; i < 9; i++) {
            float angle = (i - 4) * 0.35f;
            float spikeX = sin(angle) * 0.12f;
            float spikeZ = -0.05f - cos(angle) * 0.08f;
            float spikeH = 0.25f + (abs(i - 4) < 2 ? 0.12f : 0.0f);
            AddCylinderRW(vertices, indices, spikeX, 1.75f, spikeZ, 0.04f, spikeH, hairR, hairG, hairB, 5);
            // Spike tip
            AddSphereRW(vertices, indices, spikeX, 1.75f + spikeH, spikeZ - 0.02f, 0.035f, hairR, hairG, hairB, 4, 5);
        }
        // Side hair
        AddCylinderRW(vertices, indices, -0.15f, 1.6f, 0.0f, 0.035f, 0.12f, hairR, hairG, hairB, 5);
        AddCylinderRW(vertices, indices, 0.15f, 1.6f, 0.0f, 0.035f, 0.12f, hairR, hairG, hairB, 5);
        
    } else {
        // ============ CELL - Perfect Form ============
        // Green bio-armor
        float greenR = 0.15f, greenG = 0.6f, greenB = 0.15f;
        // Dark green spots
        float darkR = 0.08f, darkG = 0.35f, darkB = 0.08f;
        // Purple accents
        float purpR = 0.4f, purpG = 0.1f, purpB = 0.5f;
        // Black crown
        float blackR = 0.05f, blackG = 0.05f, blackB = 0.05f;
        // Skin/face
        float faceR = 0.85f, faceG = 0.75f, faceB = 0.65f;
        
        // HEAD (lighter color for face area)
        AddSphereRW(vertices, indices, 0.0f, 1.7f, 0.0f, 0.17f, faceR, faceG, faceB, 10, 14);
        
        // CELL FACE FEATURES (menacing insect-like)
        // Eyes (pink/magenta Cell eyes)
        AddSphereRW(vertices, indices, -0.055f, 1.73f, -0.13f, 0.035f, 0.95f, 0.4f, 0.6f, 5, 6); // Left eye pink
        AddSphereRW(vertices, indices, 0.055f, 1.73f, -0.13f, 0.035f, 0.95f, 0.4f, 0.6f, 5, 6);  // Right eye pink
        AddSphereRW(vertices, indices, -0.055f, 1.73f, -0.15f, 0.018f, 0.05f, 0.05f, 0.05f, 4, 5); // Left pupil (slit)
        AddSphereRW(vertices, indices, 0.055f, 1.73f, -0.15f, 0.018f, 0.05f, 0.05f, 0.05f, 4, 5);  // Right pupil
        // Black outline around eyes
        AddCylinderRW(vertices, indices, -0.055f, 1.73f, -0.12f, 0.04f, 0.01f, blackR, blackG, blackB, 6);
        AddCylinderRW(vertices, indices, 0.055f, 1.73f, -0.12f, 0.04f, 0.01f, blackR, blackG, blackB, 6);
        // Nose (small bump)
        AddSphereRW(vertices, indices, 0.0f, 1.67f, -0.14f, 0.02f, faceR * 0.9f, faceG * 0.9f, faceB * 0.9f, 4, 4);
        // Mouth (menacing smirk with dark lines)
        AddBoxRW(vertices, indices, 0.0f, 1.62f, -0.13f, 0.08f, 0.02f, 0.02f, 0.3f, 0.1f, 0.15f);
        // Black chin/jaw lines (signature Cell look)
        AddBoxRW(vertices, indices, -0.04f, 1.60f, -0.11f, 0.015f, 0.05f, 0.02f, blackR, blackG, blackB);
        AddBoxRW(vertices, indices, 0.04f, 1.60f, -0.11f, 0.015f, 0.05f, 0.02f, blackR, blackG, blackB);
        
        // CROWN/CREST - Black helmet top
        AddSphereRW(vertices, indices, 0.0f, 1.82f, -0.05f, 0.14f, blackR, blackG, blackB, 8, 10);
        // Crown spikes
        for (int i = 0; i < 5; i++) {
            float angle = (i - 2) * 0.45f;
            float spikeX = sin(angle) * 0.1f;
            float spikeH = 0.2f + (i == 2 ? 0.1f : 0.0f);
            AddCylinderRW(vertices, indices, spikeX, 1.88f, -0.08f, 0.03f, spikeH, blackR, blackG, blackB, 5);
        }
        
        // NECK (green)
        AddCylinderRW(vertices, indices, 0.0f, 1.4f, 0.0f, 0.1f, 0.18f, greenR, greenG, greenB);
        
        // TORSO - chest (green with purple chest plate)
        AddBoxRW(vertices, indices, 0.0f, 1.1f, 0.0f, 0.55f, 0.55f, 0.3f, greenR, greenG, greenB);
        // Purple chest plate
        AddBoxRW(vertices, indices, 0.0f, 1.15f, -0.16f, 0.35f, 0.35f, 0.05f, purpR, purpG, purpB);
        
        // ABDOMEN (dark green segments)
        AddBoxRW(vertices, indices, 0.0f, 0.72f, 0.0f, 0.42f, 0.22f, 0.24f, darkR, darkG, darkB);
        
        // WAIST (green)
        AddBoxRW(vertices, indices, 0.0f, 0.52f, 0.0f, 0.44f, 0.18f, 0.26f, greenR, greenG, greenB);
        
        // SHOULDERS (large pauldrons)
        AddSphereRW(vertices, indices, -0.38f, 1.28f, 0.0f, 0.14f, greenR, greenG, greenB, 7, 9);
        AddSphereRW(vertices, indices, 0.38f, 1.28f, 0.0f, 0.14f, greenR, greenG, greenB, 7, 9);
        
        // UPPER ARMS (green)
        AddCylinderRW(vertices, indices, -0.42f, 0.95f, 0.0f, 0.085f, 0.32f, greenR, greenG, greenB);
        AddCylinderRW(vertices, indices, 0.42f, 0.95f, 0.0f, 0.085f, 0.32f, greenR, greenG, greenB);
        
        // ELBOWS (dark)
        AddSphereRW(vertices, indices, -0.42f, 0.95f, 0.0f, 0.075f, darkR, darkG, darkB, 5, 6);
        AddSphereRW(vertices, indices, 0.42f, 0.95f, 0.0f, 0.075f, darkR, darkG, darkB, 5, 6);
        
        // FOREARMS (green)
        AddCylinderRW(vertices, indices, -0.42f, 0.62f, 0.0f, 0.07f, 0.33f, greenR, greenG, greenB);
        AddCylinderRW(vertices, indices, 0.42f, 0.62f, 0.0f, 0.07f, 0.33f, greenR, greenG, greenB);
        
        // HANDS (dark green) - palm
        AddSphereRW(vertices, indices, -0.42f, 0.58f, 0.0f, 0.06f, darkR, darkG, darkB, 5, 6);
        AddSphereRW(vertices, indices, 0.42f, 0.58f, 0.0f, 0.06f, darkR, darkG, darkB, 5, 6);
        // CELL CLAW FINGERS - Left hand (pointed/sharp)
        AddCylinderRW(vertices, indices, -0.45f, 0.50f, -0.015f, 0.012f, 0.07f, blackR, blackG, blackB, 4); // thumb claw
        AddCylinderRW(vertices, indices, -0.40f, 0.48f, -0.025f, 0.01f, 0.08f, blackR, blackG, blackB, 4); // index claw
        AddCylinderRW(vertices, indices, -0.42f, 0.47f, -0.005f, 0.01f, 0.085f, blackR, blackG, blackB, 4); // middle claw
        AddCylinderRW(vertices, indices, -0.44f, 0.48f, 0.015f, 0.01f, 0.08f, blackR, blackG, blackB, 4);  // ring claw
        AddCylinderRW(vertices, indices, -0.46f, 0.49f, 0.03f, 0.009f, 0.065f, blackR, blackG, blackB, 4); // pinky claw
        // CELL CLAW FINGERS - Right hand
        AddCylinderRW(vertices, indices, 0.45f, 0.50f, -0.015f, 0.012f, 0.07f, blackR, blackG, blackB, 4); // thumb claw
        AddCylinderRW(vertices, indices, 0.40f, 0.48f, -0.025f, 0.01f, 0.08f, blackR, blackG, blackB, 4); // index claw
        AddCylinderRW(vertices, indices, 0.42f, 0.47f, -0.005f, 0.01f, 0.085f, blackR, blackG, blackB, 4); // middle claw
        AddCylinderRW(vertices, indices, 0.44f, 0.48f, 0.015f, 0.01f, 0.08f, blackR, blackG, blackB, 4);  // ring claw
        AddCylinderRW(vertices, indices, 0.46f, 0.49f, 0.03f, 0.009f, 0.065f, blackR, blackG, blackB, 4); // pinky claw
        
        // UPPER LEGS (green)
        AddCylinderRW(vertices, indices, -0.14f, 0.08f, 0.0f, 0.1f, 0.38f, greenR, greenG, greenB);
        AddCylinderRW(vertices, indices, 0.14f, 0.08f, 0.0f, 0.1f, 0.38f, greenR, greenG, greenB);
        
        // KNEES (dark segments)
        AddSphereRW(vertices, indices, -0.14f, 0.08f, 0.0f, 0.085f, darkR, darkG, darkB, 5, 6);
        AddSphereRW(vertices, indices, 0.14f, 0.08f, 0.0f, 0.085f, darkR, darkG, darkB, 5, 6);
        
        // LOWER LEGS (green)
        AddCylinderRW(vertices, indices, -0.14f, -0.28f, 0.0f, 0.08f, 0.36f, greenR, greenG, greenB);
        AddCylinderRW(vertices, indices, 0.14f, -0.28f, 0.0f, 0.08f, 0.36f, greenR, greenG, greenB);
        
        // FEET (dark green - claw-like)
        AddBoxRW(vertices, indices, -0.14f, -0.38f, 0.04f, 0.14f, 0.12f, 0.22f, darkR, darkG, darkB);
        AddBoxRW(vertices, indices, 0.14f, -0.38f, 0.04f, 0.14f, 0.12f, 0.22f, darkR, darkG, darkB);
        
        // WINGS (small back wings - Cell's signature look)
        AddBoxRW(vertices, indices, -0.2f, 1.15f, 0.2f, 0.08f, 0.25f, 0.15f, blackR, blackG, blackB);
        AddBoxRW(vertices, indices, 0.2f, 1.15f, 0.2f, 0.08f, 0.25f, 0.15f, blackR, blackG, blackB);
        
        // TAIL (curled behind - spotted)
        AddCylinderRW(vertices, indices, 0.0f, 0.5f, 0.2f, 0.05f, 0.4f, greenR, greenG, greenB, 6);
        AddSphereRW(vertices, indices, 0.0f, 0.9f, 0.25f, 0.06f, darkR, darkG, darkB, 4, 5);
    }
    
    indexCount = indices.size();
    
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

static void CreateKamehaBeamMesh(GLuint& vao, GLuint& vbo, GLuint& ebo, int& indexCount,
                                  float r, float g, float b) {
    // Create a cylindrical beam shape
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    int segments = 12;
    float length = 1.0f;
    float radius = 0.3f;
    
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * 6.28318f;
        
        // Back end
        vertices.push_back(cos(angle) * radius);
        vertices.push_back(sin(angle) * radius);
        vertices.push_back(0.0f);
        vertices.push_back(cos(angle));
        vertices.push_back(sin(angle));
        vertices.push_back(0.0f);
        vertices.push_back(r);
        vertices.push_back(g);
        vertices.push_back(b);
        
        // Front end
        vertices.push_back(cos(angle) * radius * 1.2f);
        vertices.push_back(sin(angle) * radius * 1.2f);
        vertices.push_back(length);
        vertices.push_back(cos(angle));
        vertices.push_back(sin(angle));
        vertices.push_back(0.0f);
        vertices.push_back(r * 1.3f);
        vertices.push_back(g * 1.3f);
        vertices.push_back(b * 1.3f);
    }
    
    for (int i = 0; i < segments; i++) {
        int base = i * 2;
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }
    
    indexCount = indices.size();
    
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

static void CreateClashBallMesh() {
    // Energy ball at the clash point - white/bright core
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    int stacks = 12, slices = 16;
    float radius = 1.0f;
    
    for (int i = 0; i <= stacks; i++) {
        float phi = 3.14159f * i / stacks;
        for (int j = 0; j <= slices; j++) {
            float theta = 6.28318f * j / slices;
            
            float x = sin(phi) * cos(theta) * radius;
            float y = cos(phi) * radius;
            float z = sin(phi) * sin(theta) * radius;
            
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            // Bright cyan/white for clash
            vertices.push_back(0.8f);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
        }
    }
    
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            int a = i * (slices + 1) + j;
            int b = a + slices + 1;
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(a + 1);
            indices.push_back(b);
            indices.push_back(b + 1);
            indices.push_back(a + 1);
        }
    }
    
    kamehaBeamIndexCount = indices.size();
    
    glGenVertexArrays(1, &clashBallVAO);
    glGenBuffers(1, &clashBallVBO);
    glGenBuffers(1, &clashBallEBO);
    
    glBindVertexArray(clashBallVAO);
    glBindBuffer(GL_ARRAY_BUFFER, clashBallVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, clashBallEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

// Create Kamehameha pose arms (BOTH arms extended forward together)
static void CreateKamehaArms(GLuint& vao, GLuint& vbo, GLuint& ebo, int& indexCount,
                              float skinR, float skinG, float skinB,
                              float giR, float giG, float giB, bool isGoku) {
    std::vector<float> verts;
    std::vector<unsigned int> inds;
    
    // LEFT ARM - extended forward (-Z direction)
    // Shoulder joint (left side offset)
    AddSphereRW(verts, inds, -0.25f, 0.0f, 0.0f, 0.1f, giR, giG, giB, 6, 8);
    // Upper arm
    AddCylinderRW(verts, inds, -0.22f, -0.05f, -0.25f, 0.08f, 0.35f, giR, giG, giB);
    // Elbow
    AddSphereRW(verts, inds, -0.18f, -0.08f, -0.48f, 0.07f, skinR, skinG, skinB, 5, 6);
    // Forearm
    AddCylinderRW(verts, inds, -0.12f, -0.1f, -0.7f, 0.065f, 0.35f, skinR, skinG, skinB);
    // Hand
    AddSphereRW(verts, inds, -0.08f, -0.1f, -0.9f, 0.06f, skinR, skinG, skinB, 6, 8);
    // Fingers cupped (Kamehameha pose)
    AddCylinderRW(verts, inds, -0.06f, -0.08f, -0.97f, 0.018f, 0.07f, skinR, skinG, skinB, 4);
    AddCylinderRW(verts, inds, -0.03f, -0.1f, -0.96f, 0.016f, 0.065f, skinR, skinG, skinB, 4);
    AddCylinderRW(verts, inds, -0.09f, -0.1f, -0.95f, 0.016f, 0.065f, skinR, skinG, skinB, 4);
    AddCylinderRW(verts, inds, -0.12f, -0.12f, -0.94f, 0.014f, 0.055f, skinR, skinG, skinB, 4);
    
    // RIGHT ARM - extended forward (-Z direction)
    // Shoulder joint (right side offset)
    AddSphereRW(verts, inds, 0.25f, 0.0f, 0.0f, 0.1f, giR, giG, giB, 6, 8);
    // Upper arm
    AddCylinderRW(verts, inds, 0.22f, -0.05f, -0.25f, 0.08f, 0.35f, giR, giG, giB);
    // Elbow
    AddSphereRW(verts, inds, 0.18f, -0.08f, -0.48f, 0.07f, skinR, skinG, skinB, 5, 6);
    // Forearm
    AddCylinderRW(verts, inds, 0.12f, -0.1f, -0.7f, 0.065f, 0.35f, skinR, skinG, skinB);
    // Hand
    AddSphereRW(verts, inds, 0.08f, -0.1f, -0.9f, 0.06f, skinR, skinG, skinB, 6, 8);
    // Fingers cupped (Kamehameha pose)
    AddCylinderRW(verts, inds, 0.06f, -0.08f, -0.97f, 0.018f, 0.07f, skinR, skinG, skinB, 4);
    AddCylinderRW(verts, inds, 0.03f, -0.1f, -0.96f, 0.016f, 0.065f, skinR, skinG, skinB, 4);
    AddCylinderRW(verts, inds, 0.09f, -0.1f, -0.95f, 0.016f, 0.065f, skinR, skinG, skinB, 4);
    AddCylinderRW(verts, inds, 0.12f, -0.12f, -0.94f, 0.014f, 0.055f, skinR, skinG, skinB, 4);
    
    // Wristbands for Goku
    if (isGoku) {
        AddCylinderRW(verts, inds, -0.1f, -0.1f, -0.75f, 0.072f, 0.06f, 0.2f, 0.3f, 0.8f);
        AddCylinderRW(verts, inds, 0.1f, -0.1f, -0.75f, 0.072f, 0.06f, 0.2f, 0.3f, 0.8f);
    }
    
    indexCount = (int)inds.size();
    
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
}

static void InitDBZFighters() {
    // Create Goku mesh (orange gi, golden hair)
    CreateDBZFighterMesh(gokuBodyVAO, gokuBodyVBO, gokuBodyEBO, gokuBodyIndexCount,
                         1.0f, 0.5f, 0.1f, true);
    
    // Create Cell mesh (green bio-armor)
    CreateDBZFighterMesh(cellBodyVAO, cellBodyVBO, cellBodyEBO, cellBodyIndexCount,
                         0.2f, 0.7f, 0.2f, false);
    
    // Create extended arms for Kamehameha pose (both arms forward)
    // Goku's arms (skin tone, orange gi)
    CreateKamehaArms(gokuArmsVAO, gokuArmsVBO, gokuArmsEBO, gokuArmsIndexCount,
                     0.95f, 0.75f, 0.6f,   // skin
                     1.0f, 0.45f, 0.1f,    // orange gi
                     true);
    // Cell's arms (face tone, green bio-armor)
    CreateKamehaArms(cellArmsVAO, cellArmsVBO, cellArmsEBO, cellArmsIndexCount,
                     0.15f, 0.6f, 0.15f,   // green skin/armor
                     0.08f, 0.35f, 0.08f,  // dark green
                     false);
    
    // Create Goku's Kamehameha beam (PURE BLUE - classic DBZ color)
    CreateKamehaBeamMesh(gokuKamehaVAO, gokuKamehaVBO, gokuKamehaEBO, kamehaBeamIndexCount,
                         0.2f, 0.4f, 1.0f);
    
    // Create Cell's Kamehameha beam (also pure blue since it's Kamehameha)
    CreateKamehaBeamMesh(cellKamehaVAO, cellKamehaVBO, cellKamehaEBO, kamehaBeamIndexCount,
                         0.2f, 0.4f, 1.0f);
    
    // Create clash ball
    CreateClashBallMesh();
    
    // Initialize Goku (left side) - starting position for battle
    goku.homePosition = glm::vec3(-55.0f, 20.0f, -60.0f);
    goku.position = goku.homePosition;
    goku.targetPos = goku.homePosition;
    goku.velocity = glm::vec3(0.0f);
    goku.bodyRotation = 1.57f;  // Facing right toward Cell
    goku.armAngle = 0.0f;
    goku.powerLevel = 1.0f;
    goku.auraIntensity = 0.8f;
    goku.isGoku = true;
    goku.chargePhase = 0.0f;
    goku.isPunching = false;
    goku.isHit = false;
    goku.hitRecoil = 0.0f;
    goku.teleportTimer = 0.0f;
    goku.punchTimer = 0.0f;
    goku.comboCount = 0;
    
    // Initialize Cell (right side)
    cell.homePosition = glm::vec3(55.0f, 20.0f, -60.0f);
    cell.position = cell.homePosition;
    cell.targetPos = cell.homePosition;
    cell.velocity = glm::vec3(0.0f);
    cell.bodyRotation = -1.57f;  // Facing left toward Goku
    cell.armAngle = 0.0f;
    cell.powerLevel = 1.0f;
    cell.auraIntensity = 0.8f;
    cell.isGoku = false;
    cell.chargePhase = 0.5f;
    cell.isPunching = false;
    cell.isHit = false;
    cell.hitRecoil = 0.0f;
    cell.teleportTimer = 0.0f;
    cell.punchTimer = 0.0f;
    cell.comboCount = 0;
    
    // Initialize battle state
    dbzBattleState = 0;
    dbzChargeTimer = 0.0f;
    dbzTeleportInterval = 0.15f;
    dbzNextTeleport = 0.5f;
    dbzPunchCount = 0;
    dbzClashPosition = glm::vec3(0.0f, 20.0f, -60.0f);
}

static int fightZone = 0;  // Track which zone fighters are in (0-7 for 8 different positions around axis)

static void ResetDBZBattle() {
    // Cycle to next fight zone around the axis
    fightZone = (fightZone + 1) % 8;
    
    // Calculate position around a circular axis (360 degrees / 8 zones = 45 degrees each)
    float angleRad = fightZone * 0.785398f;  // 45 degrees in radians
    float fightRadius = 55.0f;  // Distance from center where fighters face off
    float heightVariation = 15.0f + (rand() / (float)RAND_MAX) * 20.0f;  // Vary height between 15-35
    
    // Center point of the fight (relative to player)
    float centerX = cos(angleRad) * 30.0f;  // Offset center around player
    float centerZ = -60.0f + sin(angleRad) * 30.0f;  // Base depth with circular offset
    float centerY = heightVariation;
    
    // Fighters face each other perpendicular to the angle from center
    float fightAngle = angleRad + 1.5708f;  // 90 degrees offset so they face each other
    
    goku.homePosition = glm::vec3(
        centerX - cos(fightAngle) * fightRadius,
        centerY + (rand() / (float)RAND_MAX - 0.5f) * 5.0f,
        centerZ - sin(fightAngle) * fightRadius
    );
    goku.position = goku.homePosition;
    goku.targetPos = goku.homePosition;
    goku.isPunching = false;
    goku.isHit = false;
    goku.hitRecoil = 0.0f;
    goku.comboCount = 0;
    goku.powerLevel = 1.0f;
    goku.auraIntensity = 0.8f;
    
    cell.homePosition = glm::vec3(
        centerX + cos(fightAngle) * fightRadius,
        centerY + (rand() / (float)RAND_MAX - 0.5f) * 5.0f,
        centerZ + sin(fightAngle) * fightRadius
    );
    cell.position = cell.homePosition;
    cell.targetPos = cell.homePosition;
    cell.isPunching = false;
    cell.isHit = false;
    cell.hitRecoil = 0.0f;
    cell.comboCount = 0;
    cell.powerLevel = 1.0f;
    cell.auraIntensity = 0.8f;
    
    // Update clash position to be between the fighters
    dbzClashPosition = glm::vec3(centerX, centerY, centerZ);
    
    punchEffects.clear();
    teleportTrails.clear();
    impactParticles.clear();
    dbzBattleState = 0;
    dbzChargeTimer = 0.0f;
    dbzNextTeleport = 0.5f;
    dbzPunchCount = 0;
}

// Helper function to spawn a random battle position around the center
static glm::vec3 GetRandomBattlePosition() {
    float centerX = 0.0f;
    float centerY = 20.0f;
    float centerZ = -60.0f;
    float range = 40.0f;
    return glm::vec3(
        centerX + (rand() / (float)RAND_MAX - 0.5f) * range * 2.0f,
        centerY + (rand() / (float)RAND_MAX - 0.5f) * 20.0f,
        centerZ + (rand() / (float)RAND_MAX - 0.5f) * range
    );
}

// Helper function to spawn punch impact effect
static void SpawnPunchEffect(glm::vec3 pos, bool isGokuPunch) {
    // Main impact burst
    PunchEffect impact;
    impact.position = pos;
    impact.life = 0.4f;
    impact.size = 3.0f;
    impact.intensity = 1.0f;
    impact.isGokuPunch = isGokuPunch;
    impact.effectType = 0;  // Impact burst
    punchEffects.push_back(impact);
    
    // Shockwave ring
    PunchEffect wave;
    wave.position = pos;
    wave.life = 0.6f;
    wave.size = 0.5f;
    wave.intensity = 0.8f;
    wave.isGokuPunch = isGokuPunch;
    wave.effectType = 1;  // Shockwave
    punchEffects.push_back(wave);
    
    // Speed lines radiating outward
    for (int i = 0; i < 8; i++) {
        PunchEffect line;
        line.position = pos;
        line.life = 0.3f;
        line.size = 1.0f + (rand() / (float)RAND_MAX) * 0.5f;
        line.intensity = 0.9f;
        line.isGokuPunch = isGokuPunch;
        line.effectType = 2;  // Speed line
        punchEffects.push_back(line);
    }
    
    // Impact particles flying outward
    for (int i = 0; i < 12; i++) {
        ImpactParticle p;
        float angle = (rand() / (float)RAND_MAX) * 6.28318f;
        float upAngle = (rand() / (float)RAND_MAX - 0.5f) * 3.14159f;
        p.position = pos;
        p.velocity = glm::vec3(
            sin(upAngle) * cos(angle) * (20.0f + rand() / (float)RAND_MAX * 15.0f),
            cos(upAngle) * 15.0f,
            sin(upAngle) * sin(angle) * (20.0f + rand() / (float)RAND_MAX * 15.0f)
        );
        p.life = 0.5f + rand() / (float)RAND_MAX * 0.3f;
        p.size = 0.3f + rand() / (float)RAND_MAX * 0.3f;
        p.isGoku = isGokuPunch;
        impactParticles.push_back(p);
    }
}

// Helper to spawn teleport trail (afterimage)
static void SpawnTeleportTrail(DBZFighter& fighter) {
    TeleportTrail trail;
    trail.position = fighter.position;
    trail.life = 0.3f;
    trail.size = 1.0f;
    trail.isGoku = fighter.isGoku;
    trail.rotation = fighter.bodyRotation;
    teleportTrails.push_back(trail);
}

static void UpdateDBZFighters(float dt) {
    // Only update in chrome world
    if (currentWorld != 2) return;
    
    goku.chargePhase += dt * 2.0f;
    cell.chargePhase += dt * 2.0f;
    
    if (dbzBattleState == 0) {
        // STATE 0: TELEPORT PUNCH BATTLE - rapid teleporting and punching
        dbzChargeTimer += dt;
        dbzNextTeleport -= dt;
        
        // Update punch timers
        if (goku.punchTimer > 0) goku.punchTimer -= dt;
        if (cell.punchTimer > 0) cell.punchTimer -= dt;
        if (goku.hitRecoil > 0) goku.hitRecoil -= dt * 3.0f;
        if (cell.hitRecoil > 0) cell.hitRecoil -= dt * 3.0f;
        
        // Time for next teleport/attack sequence
        if (dbzNextTeleport <= 0.0f) {
            dbzNextTeleport = dbzTeleportInterval;
            dbzPunchCount++;
            
            // Decide who attacks (alternating with some randomness)
            bool gokuAttacks = (dbzPunchCount % 3 != 0) ? (rand() % 2 == 0) : (dbzPunchCount % 2 == 0);
            
            // Spawn afterimage trails before teleporting
            SpawnTeleportTrail(goku);
            SpawnTeleportTrail(cell);
            
            // Generate new battle position
            glm::vec3 battlePos = GetRandomBattlePosition();
            
            if (gokuAttacks) {
                // Goku teleports to attack Cell
                goku.targetPos = battlePos + glm::vec3(-3.0f, 0.0f, 0.0f);  // Goku on left
                cell.targetPos = battlePos + glm::vec3(3.0f, 0.0f, 0.0f);   // Cell on right
                
                // Goku punches
                goku.isPunching = true;
                goku.punchTimer = 0.2f;
                cell.isHit = true;
                cell.hitRecoil = 0.3f;
                
                // Impact at Cell's position
                SpawnPunchEffect(battlePos + glm::vec3(2.0f, 1.5f, 0.0f), true);
            } else {
                // Cell teleports to attack Goku
                cell.targetPos = battlePos + glm::vec3(3.0f, 0.0f, 0.0f);   // Cell on right
                goku.targetPos = battlePos + glm::vec3(-3.0f, 0.0f, 0.0f);  // Goku on left
                
                // Cell punches
                cell.isPunching = true;
                cell.punchTimer = 0.2f;
                goku.isHit = true;
                goku.hitRecoil = 0.3f;
                
                // Impact at Goku's position
                SpawnPunchEffect(battlePos + glm::vec3(-2.0f, 1.5f, 0.0f), false);
            }
            
            // Instant teleport to new positions
            goku.position = goku.targetPos;
            cell.position = cell.targetPos;
            
            // Face each other
            glm::vec3 toCell = cell.position - goku.position;
            goku.bodyRotation = atan2(toCell.x, toCell.z);
            cell.bodyRotation = atan2(-toCell.x, -toCell.z);
            
            // After many punches, chance for CLASH (both punch at same time)
            if (dbzPunchCount >= 30 && rand() % 5 == 0) {
                dbzBattleState = 1;  // Go to clash state
                dbzClashTimer = 0.0f;
                dbzClashPosition = (goku.position + cell.position) * 0.5f;
                goku.isPunching = true;
                cell.isPunching = true;
                goku.punchTimer = 0.5f;
                cell.punchTimer = 0.5f;
                
                // Big clash effect
                SpawnPunchEffect(dbzClashPosition, true);
                SpawnPunchEffect(dbzClashPosition, false);
            }
            
            // Speed up over time
            if (dbzTeleportInterval > 0.08f) {
                dbzTeleportInterval -= 0.002f;
            }
        }
        
        // Arm angles based on punching/hit state
        if (goku.isPunching) {
            goku.armAngle = glm::mix(goku.armAngle, 120.0f, dt * 30.0f);  // Punch forward
        } else if (goku.isHit && goku.hitRecoil > 0) {
            goku.armAngle = glm::mix(goku.armAngle, -30.0f, dt * 20.0f);  // Recoil back
        } else {
            goku.armAngle = glm::mix(goku.armAngle, 45.0f, dt * 10.0f);   // Fighting stance
        }
        
        if (cell.isPunching) {
            cell.armAngle = glm::mix(cell.armAngle, 120.0f, dt * 30.0f);
        } else if (cell.isHit && cell.hitRecoil > 0) {
            cell.armAngle = glm::mix(cell.armAngle, -30.0f, dt * 20.0f);
        } else {
            cell.armAngle = glm::mix(cell.armAngle, 45.0f, dt * 10.0f);
        }
        
        // Reset punch states
        if (goku.punchTimer <= 0) goku.isPunching = false;
        if (cell.punchTimer <= 0) cell.isPunching = false;
        if (goku.hitRecoil <= 0) goku.isHit = false;
        if (cell.hitRecoil <= 0) cell.isHit = false;
        
        // Spawn aura particles
        if (rand() % 4 == 0) {
            DBZAuraParticle aura;
            aura.position = goku.position + glm::vec3(
                (rand() / (float)RAND_MAX - 0.5f) * 2.0f,
                (rand() / (float)RAND_MAX) * 3.0f,
                (rand() / (float)RAND_MAX - 0.5f) * 2.0f
            );
            aura.velocity = glm::vec3(0.0f, 5.0f + rand() / (float)RAND_MAX * 3.0f, 0.0f);
            aura.life = 0.6f;
            aura.size = 0.3f + rand() / (float)RAND_MAX * 0.3f;
            aura.isGoku = true;
            dbzAuraParticles.push_back(aura);
            
            aura.position = cell.position + glm::vec3(
                (rand() / (float)RAND_MAX - 0.5f) * 2.0f,
                (rand() / (float)RAND_MAX) * 3.0f,
                (rand() / (float)RAND_MAX - 0.5f) * 2.0f
            );
            aura.isGoku = false;
            dbzAuraParticles.push_back(aura);
        }
        
    } else if (dbzBattleState == 1) {
        // STATE 1: CLASH - both punches connect simultaneously!
        dbzClashTimer += dt;
        
        // Both fighters locked in clash, pushing against each other
        float clashShake = sin(dbzClashTimer * 40.0f) * (0.5f + dbzClashTimer * 2.0f);
        goku.position = dbzClashPosition + glm::vec3(-4.0f + clashShake, clashShake * 0.5f, 0.0f);
        cell.position = dbzClashPosition + glm::vec3(4.0f - clashShake, -clashShake * 0.5f, 0.0f);
        
        // Arms locked in punch position
        goku.armAngle = 100.0f + sin(dbzClashTimer * 20.0f) * 10.0f;
        cell.armAngle = 100.0f - sin(dbzClashTimer * 20.0f) * 10.0f;
        
        // Growing energy at clash point
        float clashEnergy = dbzClashTimer * 3.0f;
        
        // Spawn clash particles
        if (rand() % 2 == 0) {
            ImpactParticle p;
            float angle = (rand() / (float)RAND_MAX) * 6.28318f;
            p.position = dbzClashPosition;
            p.velocity = glm::vec3(cos(angle) * 15.0f, (rand() / (float)RAND_MAX) * 10.0f, sin(angle) * 15.0f);
            p.life = 0.5f;
            p.size = 0.4f + clashEnergy * 0.1f;
            p.isGoku = (rand() % 2 == 0);
            impactParticles.push_back(p);
        }
        
        // After clash duration, BIG EXPLOSION
        if (dbzClashTimer > 2.0f) {
            dbzBattleState = 2;
            dbzReturnTimer = 0.0f;
            
            // Massive explosion effects
            for (int i = 0; i < 30; i++) {
                ImpactParticle p;
                float angle = (rand() / (float)RAND_MAX) * 6.28318f;
                float upAngle = (rand() / (float)RAND_MAX) * 3.14159f;
                float speed = 40.0f + rand() / (float)RAND_MAX * 30.0f;
                p.position = dbzClashPosition;
                p.velocity = glm::vec3(
                    sin(upAngle) * cos(angle) * speed,
                    cos(upAngle) * speed * 0.5f,
                    sin(upAngle) * sin(angle) * speed
                );
                p.life = 1.5f;
                p.size = 0.5f + rand() / (float)RAND_MAX * 0.5f;
                p.isGoku = (rand() % 2 == 0);
                impactParticles.push_back(p);
            }
            
            // Big shockwave effect
            PunchEffect bigWave;
            bigWave.position = dbzClashPosition;
            bigWave.life = 1.5f;
            bigWave.size = 1.0f;
            bigWave.intensity = 1.0f;
            bigWave.isGokuPunch = true;
            bigWave.effectType = 1;
            punchEffects.push_back(bigWave);
        }
        
    } else if (dbzBattleState == 2) {
        // STATE 2: BIG EXPLOSION - fighters blown apart
        dbzReturnTimer += dt;
        
        // Push fighters away from clash point
        glm::vec3 gokuFromClash = goku.position - dbzClashPosition;
        glm::vec3 cellFromClash = cell.position - dbzClashPosition;
        
        if (glm::length(gokuFromClash) > 0.1f && glm::length(gokuFromClash) < 80.0f) {
            goku.position += glm::normalize(gokuFromClash) * dt * 100.0f;
        }
        if (glm::length(cellFromClash) > 0.1f && glm::length(cellFromClash) < 80.0f) {
            cell.position += glm::normalize(cellFromClash) * dt * 100.0f;
        }
        
        // Tumbling animation
        goku.bodyRotation += dt * 8.0f;
        cell.bodyRotation -= dt * 8.0f;
        
        // After explosion settles, return
        if (dbzReturnTimer > 2.5f) {
            dbzBattleState = 3;
            dbzReturnTimer = 0.0f;
        }
        
    } else if (dbzBattleState == 3) {
        // STATE 3: RETURNING to positions
        dbzReturnTimer += dt;
        
        float returnSpeed = 3.0f;
        goku.position = glm::mix(goku.position, goku.homePosition, dt * returnSpeed);
        cell.position = glm::mix(cell.position, cell.homePosition, dt * returnSpeed);
        
        // Face each other
        glm::vec3 toCell = cell.position - goku.position;
        goku.bodyRotation = glm::mix(goku.bodyRotation, atan2(toCell.x, toCell.z), dt * 5.0f);
        cell.bodyRotation = glm::mix(cell.bodyRotation, atan2(-toCell.x, -toCell.z), dt * 5.0f);
        
        // Arms return to neutral
        goku.armAngle = glm::mix(goku.armAngle, 0.0f, dt * 4.0f);
        cell.armAngle = glm::mix(cell.armAngle, 0.0f, dt * 4.0f);
        
        float gokuDist = glm::length(goku.position - goku.homePosition);
        float cellDist = glm::length(cell.position - cell.homePosition);
        
        if ((gokuDist < 2.0f && cellDist < 2.0f) || dbzReturnTimer > 3.0f) {
            ResetDBZBattle();
        }
    }
    
    // Update aura particles
    for (size_t i = 0; i < dbzAuraParticles.size(); ) {
        dbzAuraParticles[i].position += dbzAuraParticles[i].velocity * dt;
        dbzAuraParticles[i].life -= dt * 1.5f;
        
        if (dbzAuraParticles[i].life <= 0.0f) {
            dbzAuraParticles.erase(dbzAuraParticles.begin() + i);
        } else {
            i++;
        }
    }
    
    // Update impact particles
    for (size_t i = 0; i < impactParticles.size(); ) {
        impactParticles[i].position += impactParticles[i].velocity * dt;
        impactParticles[i].velocity.y -= dt * 20.0f;  // Gravity
        impactParticles[i].life -= dt * 1.5f;
        
        if (impactParticles[i].life <= 0.0f) {
            impactParticles.erase(impactParticles.begin() + i);
        } else {
            i++;
        }
    }
    
    // Update punch effects
    for (size_t i = 0; i < punchEffects.size(); ) {
        punchEffects[i].life -= dt;
        
        // Shockwaves expand
        if (punchEffects[i].effectType == 1) {
            punchEffects[i].size += dt * 40.0f;
        }
        
        if (punchEffects[i].life <= 0.0f) {
            punchEffects.erase(punchEffects.begin() + i);
        } else {
            i++;
        }
    }
    
    // Update teleport trails (afterimages fade)
    for (size_t i = 0; i < teleportTrails.size(); ) {
        teleportTrails[i].life -= dt * 3.0f;
        
        if (teleportTrails[i].life <= 0.0f) {
            teleportTrails.erase(teleportTrails.begin() + i);
        } else {
            i++;
        }
    }
    
    // Limit particles
    while (dbzAuraParticles.size() > 300) {
        dbzAuraParticles.erase(dbzAuraParticles.begin());
    }
    while (impactParticles.size() > 400) {
        impactParticles.erase(impactParticles.begin());
    }
    while (punchEffects.size() > 100) {
        punchEffects.erase(punchEffects.begin());
    }
    while (teleportTrails.size() > 20) {
        teleportTrails.erase(teleportTrails.begin());
    }
}

// ---------------------------------------------------------------------------
// Wind particles (floating debris to show motion)
// ---------------------------------------------------------------------------
static GLuint particleVAO = 0, particleVBO = 0;
static GLuint particleProgram = 0;
static const int NUM_PARTICLES = 500;
static std::vector<glm::vec3> particlePositions;

static void InitParticles() {
    particlePositions.resize(NUM_PARTICLES);
    for (int i = 0; i < NUM_PARTICLES; i++) {
        particlePositions[i] = glm::vec3(
            (rand() / (float)RAND_MAX - 0.5f) * 100.0f,
            (rand() / (float)RAND_MAX - 0.5f) * 100.0f,
            (rand() / (float)RAND_MAX - 0.5f) * 100.0f
        );
    }
    
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);
    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, NUM_PARTICLES * sizeof(glm::vec3), particlePositions.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
    
    const char* vsSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 uVP;
        uniform vec3 uCharPos;
        out float vAlpha;
        void main() {
            vec3 pos = aPos + uCharPos;
            gl_Position = uVP * vec4(pos, 1.0);
            gl_PointSize = 3.0;
            float dist = length(aPos);
            vAlpha = 1.0 - smoothstep(30.0, 50.0, dist);
        }
    )";
    
    const char* fsSource = R"(
        #version 330 core
        in float vAlpha;
        out vec4 FragColor;
        void main() {
            vec2 coord = gl_PointCoord - vec2(0.5);
            float r = length(coord);
            if (r > 0.5) discard;
            float alpha = (1.0 - r * 2.0) * vAlpha * 0.6;
            FragColor = vec4(1.0, 1.0, 1.0, alpha);
        }
    )";
    
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSource, nullptr);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSource, nullptr);
    glCompileShader(fs);
    
    particleProgram = glCreateProgram();
    glAttachShader(particleProgram, vs);
    glAttachShader(particleProgram, fs);
    glLinkProgram(particleProgram);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
}

static void UpdateParticles(float dt) {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        // Move particles upward relative to falling character
        particlePositions[i].y += characterVel.y * dt * -0.5f + 20.0f * dt;
        
        // Wrap particles around
        if (particlePositions[i].y > 50.0f) particlePositions[i].y -= 100.0f;
        if (particlePositions[i].y < -50.0f) particlePositions[i].y += 100.0f;
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, NUM_PARTICLES * sizeof(glm::vec3), particlePositions.data());
}

// ---------------------------------------------------------------------------
// Ground Grid (to show movement)
// ---------------------------------------------------------------------------
static GLuint gridVAO = 0, gridVBO = 0;
static GLuint gridProgram = 0;
static int gridVertexCount = 0;

static void InitGrid() {
    // Create grid lines
    std::vector<float> verts;
    float gridSize = 200.0f;
    float gridSpacing = 10.0f;
    float gridY = -5.0f;  // Below character
    
    for (float x = -gridSize; x <= gridSize; x += gridSpacing) {
        // Line along Z
        verts.push_back(x); verts.push_back(gridY); verts.push_back(-gridSize);
        verts.push_back(x); verts.push_back(gridY); verts.push_back(gridSize);
    }
    for (float z = -gridSize; z <= gridSize; z += gridSpacing) {
        // Line along X
        verts.push_back(-gridSize); verts.push_back(gridY); verts.push_back(z);
        verts.push_back(gridSize); verts.push_back(gridY); verts.push_back(z);
    }
    
    gridVertexCount = verts.size() / 3;
    
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Simple grid shader
    const char* vsSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 uVP;
        out float vDist;
        void main() {
            gl_Position = uVP * vec4(aPos, 1.0);
            vDist = length(aPos.xz);
        }
    )";
    
    const char* fsSource = R"(
        #version 330 core
        in float vDist;
        out vec4 FragColor;
        uniform int uWorldType;
        void main() {
            float alpha = 1.0 - smoothstep(50.0, 200.0, vDist);
            vec3 color = vec3(0.3, 0.5, 0.8);
            if (uWorldType == 1) color = vec3(0.5, 0.3, 0.8);
            else if (uWorldType == 2) color = vec3(0.8, 0.8, 0.8);
            else if (uWorldType == 3) color = vec3(1.0, 0.8, 0.2);
            else if (uWorldType == 4) color = vec3(0.8, 0.6, 0.3);
            FragColor = vec4(color, alpha * 0.5);
        }
    )";
    
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSource, nullptr);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSource, nullptr);
    glCompileShader(fs);
    
    gridProgram = glCreateProgram();
    glAttachShader(gridProgram, vs);
    glAttachShader(gridProgram, fs);
    glLinkProgram(gridProgram);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
}

// ---------------------------------------------------------------------------
// Portal
// ---------------------------------------------------------------------------
static GLuint portalVAO = 0, portalVBO = 0;
static GLuint portalProgram = 0;
static float portalY = -30.0f;  // Portal position (below character)
static const float PORTAL_RADIUS = 8.0f;
static const int PORTAL_SEGMENTS = 64;

static void InitPortal() {
    // Create a disc for the portal
    std::vector<float> verts;
    
    // Center vertex
    verts.push_back(0.0f);
    verts.push_back(0.0f);
    verts.push_back(0.0f);
    
    // Ring vertices
    for (int i = 0; i <= PORTAL_SEGMENTS; i++) {
        float angle = (float)i / PORTAL_SEGMENTS * 2.0f * 3.14159f;
        verts.push_back(cos(angle) * PORTAL_RADIUS);
        verts.push_back(0.0f);
        verts.push_back(sin(angle) * PORTAL_RADIUS);
    }
    
    glGenVertexArrays(1, &portalVAO);
    glGenBuffers(1, &portalVBO);
    glBindVertexArray(portalVAO);
    glBindBuffer(GL_ARRAY_BUFFER, portalVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    
    const char* vsSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        
        out vec3 vLocalPos;
        
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;
        
        void main() {
            vLocalPos = aPos;
            gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
        }
    )";
    
    const char* fsSource = R"(
        #version 330 core
        in vec3 vLocalPos;
        out vec4 FragColor;
        
        uniform float uTime;
        uniform int uWorldType;
        uniform float uRadius;
        uniform float uFadeAlpha;
        
        void main() {
            float dist = length(vLocalPos.xz) / uRadius;
            
            // Swirling pattern
            float angle = atan(vLocalPos.z, vLocalPos.x);
            float swirl = sin(angle * 5.0 - uTime * 3.0 + dist * 10.0);
            swirl = swirl * 0.5 + 0.5;
            
            // Rings
            float rings = sin(dist * 20.0 - uTime * 5.0);
            rings = rings * 0.5 + 0.5;
            
            // Energy glow
            float glow = 1.0 - dist;
            glow = pow(glow, 0.5);
            
            vec3 color1, color2;
            if (uWorldType == 0) {
                // Day world portals
                if (uRadius > 7.0) {
                    // Day->Night portal - purple/blue
                    color1 = vec3(0.2, 0.1, 0.8);
                    color2 = vec3(0.6, 0.3, 1.0);
                } else if (uRadius > 5.5) {
                    // Day->Mirror portal - silver/chrome
                    color1 = vec3(0.7, 0.75, 0.8);
                    color2 = vec3(1.0, 1.0, 1.0);
                } else if (uRadius > 4.5) {
                    // Day->Hall of Mirrors portal - gold/elegant
                    color1 = vec3(0.8, 0.6, 0.1);
                    color2 = vec3(1.0, 0.85, 0.3);
                } else {
                    // Day->Speedforce portal - red/orange lightning
                    color1 = vec3(1.0, 0.2, 0.0);
                    color2 = vec3(1.0, 0.8, 0.0);
                }
            } else if (uWorldType == 1) {
                if (uRadius > 7.0) {
                    color1 = vec3(1.0, 0.5, 0.1);
                    color2 = vec3(1.0, 0.9, 0.4);
                } else if (uRadius > 5.5) {
                    color1 = vec3(0.6, 0.65, 0.75);
                    color2 = vec3(0.9, 0.95, 1.0);
                } else if (uRadius > 4.5) {
                    color1 = vec3(0.85, 0.65, 0.15);
                    color2 = vec3(1.0, 0.9, 0.4);
                } else {
                    color1 = vec3(1.0, 0.3, 0.0);
                    color2 = vec3(1.0, 0.7, 0.2);
                }
            } else if (uWorldType == 2) {
                if (uRadius > 7.0) {
                    color1 = vec3(1.0, 0.7, 0.2);
                    color2 = vec3(1.0, 0.95, 0.6);
                } else if (uRadius > 5.5) {
                    color1 = vec3(0.3, 0.1, 0.6);
                    color2 = vec3(0.5, 0.2, 0.9);
                } else if (uRadius > 4.5) {
                    color1 = vec3(0.9, 0.7, 0.2);
                    color2 = vec3(1.0, 0.85, 0.4);
                } else {
                    color1 = vec3(0.9, 0.2, 0.0);
                    color2 = vec3(1.0, 0.6, 0.1);
                }
            } else if (uWorldType == 3) {
                // Speedforce world portals
                if (uRadius > 7.0) {
                    // Speedforce->Day - calm blue
                    color1 = vec3(0.2, 0.5, 1.0);
                    color2 = vec3(0.5, 0.8, 1.0);
                } else if (uRadius > 5.5) {
                    // Speedforce->Night - purple
                    color1 = vec3(0.4, 0.1, 0.8);
                    color2 = vec3(0.6, 0.3, 1.0);
                } else if (uRadius > 4.5) {
                    // Speedforce->Hall of Mirrors - gold
                    color1 = vec3(0.85, 0.6, 0.1);
                    color2 = vec3(1.0, 0.8, 0.3);
                } else {
                    // Speedforce->Mirror - white/silver
                    color1 = vec3(0.8, 0.8, 0.9);
                    color2 = vec3(1.0, 1.0, 1.0);
                }
            } else {
                // Hall of Mirrors world portals - elegant colors
                if (uRadius > 7.0) {
                    // Hall->Day - warm golden sunrise
                    color1 = vec3(1.0, 0.6, 0.2);
                    color2 = vec3(1.0, 0.9, 0.5);
                } else if (uRadius > 5.5) {
                    // Hall->Night - deep purple
                    color1 = vec3(0.3, 0.1, 0.5);
                    color2 = vec3(0.5, 0.3, 0.8);
                } else if (uRadius > 4.5) {
                    // Hall->Chrome - silver
                    color1 = vec3(0.7, 0.75, 0.85);
                    color2 = vec3(1.0, 1.0, 1.0);
                } else {
                    // Hall->Speedforce - red lightning
                    color1 = vec3(1.0, 0.2, 0.0);
                    color2 = vec3(1.0, 0.7, 0.1);
                }
            }
            
            vec3 color = mix(color1, color2, swirl * rings);
            color += vec3(1.0) * pow(1.0 - dist, 3.0) * 0.5;  // Bright center
            
            // Edge glow
            float edge = smoothstep(0.8, 1.0, dist);
            color += (uWorldType == 0 ? vec3(0.5, 0.3, 1.0) : vec3(1.0, 0.6, 0.2)) * edge * 2.0;
            
            // Fade at edges
            float alpha = smoothstep(1.0, 0.7, dist) * glow;
            alpha = max(alpha, edge * 0.8);
            
            // Apply fade-in alpha
            alpha *= uFadeAlpha;
            
            FragColor = vec4(color, alpha);
        }
    )";
    
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSource, nullptr);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSource, nullptr);
    glCompileShader(fs);
    
    portalProgram = glCreateProgram();
    glAttachShader(portalProgram, vs);
    glAttachShader(portalProgram, fs);
    glLinkProgram(portalProgram);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
}

// ---------------------------------------------------------------------------
// Mirror Planes (for Hall of Mirrors world)
// ---------------------------------------------------------------------------
static void InitMirrors() {
    // Create a large flat quad for mirrors
    float mirrorVerts[] = {
        // Position (3), Normal (3), TexCoord (2)
        -MIRROR_WIDTH/2, -MIRROR_HEIGHT/2, 0,  0, 0, 1,  0, 0,
         MIRROR_WIDTH/2, -MIRROR_HEIGHT/2, 0,  0, 0, 1,  1, 0,
         MIRROR_WIDTH/2,  MIRROR_HEIGHT/2, 0,  0, 0, 1,  1, 1,
        -MIRROR_WIDTH/2, -MIRROR_HEIGHT/2, 0,  0, 0, 1,  0, 0,
         MIRROR_WIDTH/2,  MIRROR_HEIGHT/2, 0,  0, 0, 1,  1, 1,
        -MIRROR_WIDTH/2,  MIRROR_HEIGHT/2, 0,  0, 0, 1,  0, 1,
    };
    
    glGenVertexArrays(1, &mirrorVAO);
    glGenBuffers(1, &mirrorVBO);
    glBindVertexArray(mirrorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mirrorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(mirrorVerts), mirrorVerts, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    
    glBindVertexArray(0);
    
    // Create framebuffers for each mirror
    for (int i = 0; i < 4; i++) {
        // Create framebuffer
        glGenFramebuffers(1, &mirrorFBO[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, mirrorFBO[i]);
        
        // Create texture
        glGenTextures(1, &mirrorTexture[i]);
        glBindTexture(GL_TEXTURE_2D, mirrorTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, MIRROR_TEX_SIZE, MIRROR_TEX_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTexture[i], 0);
        
        // Create depth renderbuffer
        glGenRenderbuffers(1, &mirrorDepthRBO[i]);
        glBindRenderbuffer(GL_RENDERBUFFER, mirrorDepthRBO[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, MIRROR_TEX_SIZE, MIRROR_TEX_SIZE);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mirrorDepthRBO[i]);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            printf("Mirror framebuffer %d not complete!\n", i);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Mirror shader - uses reflection texture
    const char* vsSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec2 aTexCoord;
        
        out vec3 vWorldPos;
        out vec3 vNormal;
        out vec2 vTexCoord;
        
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;
        
        void main() {
            vec4 worldPos = uModel * vec4(aPos, 1.0);
            vWorldPos = worldPos.xyz;
            vNormal = mat3(uModel) * aNormal;
            vTexCoord = aTexCoord;
            gl_Position = uProj * uView * worldPos;
        }
    )";
    
    const char* fsSource = R"(
        #version 330 core
        in vec3 vWorldPos;
        in vec3 vNormal;
        in vec2 vTexCoord;
        
        out vec4 FragColor;
        
        uniform vec3 uCameraPos;
        uniform float uTime;
        uniform sampler2D uReflectionTex;
        uniform int uUseReflectionTex;
        uniform int uMirrorType;  // 0=normal, 1=enlarged/magnified, 2=stretched/funhouse, 3=normal flipped
        
        void main() {
            // Thin elegant frame border
            float borderX = smoothstep(0.0, 0.02, vTexCoord.x) * smoothstep(1.0, 0.98, vTexCoord.x);
            float borderY = smoothstep(0.0, 0.02, vTexCoord.y) * smoothstep(1.0, 0.98, vTexCoord.y);
            float border = 1.0 - (borderX * borderY);
            vec3 frameColor = vec3(0.75, 0.6, 0.25);  // Gold frame
            
            vec3 mirrorColor;
            float alpha;
            
            if (uUseReflectionTex == 1 && border < 0.5) {
                // Calculate distorted UV based on mirror type
                vec2 reflectUV = vec2(vTexCoord.x, 1.0 - vTexCoord.y);
                vec2 center = vec2(0.5, 0.5);
                vec2 fromCenter = reflectUV - center;
                
                if (uMirrorType == 1) {
                    // ENLARGED/MAGNIFIED - zoom into center (convex mirror effect)
                    float zoom = 2.5;  // Magnification factor
                    reflectUV = center + fromCenter / zoom;
                    // Clamp to valid range
                    reflectUV = clamp(reflectUV, 0.0, 1.0);
                } else if (uMirrorType == 2) {
                    // STRETCHED/FUNHOUSE - vertical stretch with wavy distortion
                    float stretchY = 2.0;  // Vertical stretch
                    float wave = sin(reflectUV.y * 6.28 + uTime * 2.0) * 0.05;
                    reflectUV.x = center.x + (fromCenter.x + wave) * 0.7;
                    reflectUV.y = center.y + fromCenter.y / stretchY;
                    reflectUV = clamp(reflectUV, 0.0, 1.0);
                } else if (uMirrorType == 3) {
                    // NORMAL but horizontally flipped
                    reflectUV.x = 1.0 - reflectUV.x;
                }
                // uMirrorType == 0 is normal, no modification needed
                
                mirrorColor = texture(uReflectionTex, reflectUV).rgb;
                
                // Add visual indicator for special mirrors
                if (uMirrorType == 1) {
                    // Slight golden tint for magnifying mirror
                    mirrorColor = mix(mirrorColor, vec3(1.0, 0.95, 0.8), 0.1);
                } else if (uMirrorType == 2) {
                    // Slight purple tint for funhouse mirror
                    mirrorColor = mix(mirrorColor, vec3(0.9, 0.8, 1.0), 0.15);
                }
                
                // Subtle fresnel effect for realism
                vec3 V = normalize(uCameraPos - vWorldPos);
                vec3 N = normalize(vNormal);
                float fresnel = pow(1.0 - max(dot(V, N), 0.0), 3.0);
                mirrorColor = mix(mirrorColor, vec3(0.9, 0.95, 1.0), fresnel * 0.3);
                
                alpha = 0.95;  // Nearly opaque reflection
            } else {
                // Fallback clear mirror look
                mirrorColor = vec3(0.85, 0.9, 0.95);
                alpha = 0.3;
            }
            
            // Combine mirror and frame - different frame colors per type
            if (uMirrorType == 1) {
                frameColor = vec3(0.85, 0.7, 0.2);  // Brighter gold for magnifying
            } else if (uMirrorType == 2) {
                frameColor = vec3(0.6, 0.4, 0.7);  // Purple for funhouse
            }
            
            vec3 finalColor = mix(mirrorColor, frameColor, border);
            alpha = mix(alpha, 0.95, border);
            
            FragColor = vec4(finalColor, alpha);
        }
    )";
    
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSource, nullptr);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSource, nullptr);
    glCompileShader(fs);
    
    mirrorProgram = glCreateProgram();
    glAttachShader(mirrorProgram, vs);
    glAttachShader(mirrorProgram, fs);
    glLinkProgram(mirrorProgram);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    // Initialize shatter particle system
    glGenVertexArrays(1, &shatterVAO);
    glGenBuffers(1, &shatterVBO);
    glBindVertexArray(shatterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, shatterVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 1000, nullptr, GL_DYNAMIC_DRAW);  // Max 1000 particles
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
    glBindVertexArray(0);
    
    // Shatter shader
    const char* shatterVS = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec2 aSize;
        layout(location = 2) in float aAlpha;
        
        out float vAlpha;
        out vec2 vSize;
        
        uniform mat4 uVP;
        
        void main() {
            gl_Position = uVP * vec4(aPos, 1.0);
            gl_PointSize = max(aSize.x, aSize.y) * 50.0 / gl_Position.w;
            vAlpha = aAlpha;
            vSize = aSize;
        }
    )";
    
    const char* shatterFS = R"(
        #version 330 core
        in float vAlpha;
        in vec2 vSize;
        out vec4 FragColor;
        
        void main() {
            vec2 coord = gl_PointCoord * 2.0 - 1.0;
            // Shard shape
            float shard = 1.0 - length(coord);
            if (shard < 0.2) discard;
            
            // Reflective glass color
            vec3 color = vec3(0.9, 0.95, 1.0);
            color += vec3(0.1) * (coord.x + coord.y);  // Slight rainbow
            
            FragColor = vec4(color, vAlpha * shard);
        }
    )";
    
    GLuint svs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(svs, 1, &shatterVS, nullptr);
    glCompileShader(svs);
    
    GLuint sfs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(sfs, 1, &shatterFS, nullptr);
    glCompileShader(sfs);
    
    shatterProgram = glCreateProgram();
    glAttachShader(shatterProgram, svs);
    glAttachShader(shatterProgram, sfs);
    glLinkProgram(shatterProgram);
    
    glDeleteShader(svs);
    glDeleteShader(sfs);
}

// Get mirror position and normal for a given mirror index
// Mirrors are at FIXED positions in the world (not following character)
static void GetMirrorInfo(int mirrorIndex, glm::vec3& outPos, glm::vec3& outNormal, float& outYaw) {
    float cornerOffset = MIRROR_DISTANCE * 0.7f;
    // Fixed positions in world space (mirrors don't follow character)
    switch (mirrorIndex) {
        case 0: // Front-left
            outPos = glm::vec3(-cornerOffset, 0, -cornerOffset);
            outNormal = glm::normalize(glm::vec3(1, 0, 1));
            outYaw = 45.0f;
            break;
        case 1: // Front-right
            outPos = glm::vec3(cornerOffset, 0, -cornerOffset);
            outNormal = glm::normalize(glm::vec3(-1, 0, 1));
            outYaw = -45.0f;
            break;
        case 2: // Back-left
            outPos = glm::vec3(-cornerOffset, 0, cornerOffset);
            outNormal = glm::normalize(glm::vec3(1, 0, -1));
            outYaw = 135.0f;
            break;
        case 3: // Back-right
            outPos = glm::vec3(cornerOffset, 0, cornerOffset);
            outNormal = glm::normalize(glm::vec3(-1, 0, -1));
            outYaw = -135.0f;
            break;
    }
}

// Render scene for mirror reflection (into framebuffer)
static void RenderMirrorReflection(int mirrorIndex, const glm::vec3& cameraPos, const glm::mat4& proj,
                                    GLuint skyboxProg, GLuint skyVAO, GLuint charProg, GLuint charVAO, 
                                    int charIdxCount, float nowT, float totalFall) {
    if (mirrorBroken[mirrorIndex]) return;
    
    glm::vec3 mirrorPos, mirrorNormal;
    float mirrorYaw;
    GetMirrorInfo(mirrorIndex, mirrorPos, mirrorNormal, mirrorYaw);
    
    // Calculate reflected camera position
    glm::vec3 toCamera = cameraPos - mirrorPos;
    float d = glm::dot(toCamera, mirrorNormal);
    glm::vec3 reflectedCamPos = cameraPos - 2.0f * d * mirrorNormal;
    
    // Look at the character's reflection
    glm::vec3 charWorldPos(characterPos.x, 0, characterPos.z);
    glm::vec3 toChar = charWorldPos - mirrorPos;
    float dChar = glm::dot(toChar, mirrorNormal);
    glm::vec3 reflectedCharPos = charWorldPos - 2.0f * dChar * mirrorNormal;
    
    // Create view matrix looking from reflected camera at reflected character
    glm::mat4 reflView = glm::lookAt(reflectedCamPos, reflectedCharPos, glm::vec3(0, 1, 0));
    glm::mat4 reflVP = proj * reflView;
    
    // Bind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, mirrorFBO[mirrorIndex]);
    glViewport(0, 0, MIRROR_TEX_SIZE, MIRROR_TEX_SIZE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Draw skybox
    glDepthFunc(GL_LEQUAL);
    glUseProgram(skyboxProg);
    glm::mat4 skyView = glm::mat4(glm::mat3(reflView));
    glm::mat4 skyVP = proj * skyView;
    glUniformMatrix4fv(glGetUniformLocation(skyboxProg, "uVP"), 1, GL_FALSE, glm::value_ptr(skyVP));
    glUniform1f(glGetUniformLocation(skyboxProg, "uTime"), nowT);
    glUniform1f(glGetUniformLocation(skyboxProg, "uFallDistance"), totalFall);
    glUniform1i(glGetUniformLocation(skyboxProg, "uWorldType"), currentWorld);
    glUniform1f(glGetUniformLocation(skyboxProg, "uTransition"), 0.0f);
    glBindVertexArray(skyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDepthFunc(GL_LESS);
    
    // Draw reflected character or bot
    if (useGLTFBot) {
        // Use animated GLTF bot model (mirror the main scene logic)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, reflectedCharPos);
        model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(15.0f), glm::vec3(1, 0, 0));
        float tiltAmount = 25.0f;
        float tiltX = glm::clamp(characterVel.z * 0.5f, -tiltAmount, tiltAmount);
        float tiltZ = glm::clamp(characterVel.x * 0.5f, -tiltAmount, tiltAmount);
        model = glm::rotate(model, glm::radians(tiltX), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(tiltZ), glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(0.04f));
        model = glm::translate(model, glm::vec3(280.0f, 105.0f, 0.0f));
        glm::mat4 mvp = proj * reflView * model;
        glm::vec3 lightPos = reflectedCharPos + glm::vec3(0.0f, 150.0f, 0.0f);
        glm::vec3 lightInt(6e6f, 5e6f, 3e6f);
        bot.render(mvp, model, reflectedCamPos, lightPos, lightInt, currentWorld);
    } else {
        glUseProgram(charProg);
        glm::mat4 charModel = glm::mat4(1.0f);
        charModel = glm::translate(charModel, reflectedCharPos);
        charModel = glm::rotate(charModel, glm::radians(-characterRotX), glm::vec3(1, 0, 0));
        charModel = glm::rotate(charModel, glm::radians(characterRotZ), glm::vec3(0, 0, 1));
        charModel = glm::scale(charModel, glm::vec3(1.5f, 1.5f, 1.5f));
        glUniformMatrix4fv(glGetUniformLocation(charProg, "uModel"), 1, GL_FALSE, glm::value_ptr(charModel));
        glUniformMatrix4fv(glGetUniformLocation(charProg, "uView"), 1, GL_FALSE, glm::value_ptr(reflView));
        glUniformMatrix4fv(glGetUniformLocation(charProg, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniform3f(glGetUniformLocation(charProg, "uLightDir"), 0.3f, 0.9f, 0.2f);
        glUniform1f(glGetUniformLocation(charProg, "uReflection"), 0.0f);
        glUniform3fv(glGetUniformLocation(charProg, "uViewPos"), 1, glm::value_ptr(reflectedCamPos));
        glBindVertexArray(charVAO);
        glDrawElements(GL_TRIANGLES, charIdxCount, GL_UNSIGNED_INT, 0);
    }
    
    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Check if all 4 mirrors are broken (triggers explosion)
static bool AllMirrorsBroken() {
    return mirrorBroken[0] && mirrorBroken[1] && mirrorBroken[2] && mirrorBroken[3];
}

// Spawn explosion particles for world destruction
static void TriggerWorldExplosion() {
    worldExploding = true;
    worldExplosionTime = 0.0f;
    
    // Spawn massive explosion particles from center
    for (int i = 0; i < 300; i++) {
        ShatterParticle p;
        
        // Random direction
        float theta = ((rand() % 1000) / 1000.0f) * 6.28318f;
        float phi = ((rand() % 1000) / 1000.0f) * 3.14159f;
        float r = 2.0f + (rand() % 100) / 10.0f;
        
        p.position = glm::vec3(0, 0, 0);
        p.velocity = glm::vec3(
            sin(phi) * cos(theta) * r * 3.0f,
            sin(phi) * sin(theta) * r * 3.0f,
            cos(phi) * r * 3.0f
        );
        
        p.size = glm::vec2(
            0.5f + (rand() % 100) / 50.0f,
            0.5f + (rand() % 100) / 50.0f
        );
        
        p.rotation = (rand() % 360) * 0.0174533f;
        p.rotSpeed = ((rand() % 1000) / 200.0f - 2.5f) * 5.0f;
        p.alpha = 1.0f;
        p.lifetime = 2.5f + (rand() % 100) / 100.0f;
        
        shatterParticles.push_back(p);
    }
    
    // Play explosion sound
    std::cout << "\a\a\a" << std::flush;
}

// Spawn shatter particles when a mirror breaks
static void BreakMirror(int mirrorIndex, glm::vec3 mirrorCenter, glm::vec3 mirrorNormal) {
    mirrorBroken[mirrorIndex] = true;
    mirrorBreakTime[mirrorIndex] = (float)glfwGetTime();
    
    // Spawn glass shatter particles - more shards for dramatic effect
    for (int i = 0; i < 150; i++) {
        ShatterParticle p;
        
        // Random position on the mirror surface
        float rx = ((rand() % 1000) / 1000.0f - 0.5f) * MIRROR_WIDTH;
        float ry = ((rand() % 1000) / 1000.0f - 0.5f) * MIRROR_HEIGHT;
        
        // Calculate position based on mirror orientation
        glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), mirrorNormal));
        if (glm::length(right) < 0.1f) right = glm::vec3(1, 0, 0);
        glm::vec3 up = glm::normalize(glm::cross(mirrorNormal, right));
        
        p.position = mirrorCenter + right * rx + up * ry;
        
        // Velocity - explode outward from impact point (character position)
        glm::vec3 toShard = p.position - glm::vec3(characterPos.x, 0, characterPos.z);
        float speed = 8.0f + (rand() % 100) / 8.0f;
        p.velocity = glm::normalize(toShard + mirrorNormal * 0.5f) * speed;
        p.velocity += glm::vec3(
            ((rand() % 1000) / 500.0f - 1.0f) * 4.0f,
            ((rand() % 1000) / 500.0f - 1.0f) * 4.0f + 3.0f,
            ((rand() % 1000) / 500.0f - 1.0f) * 4.0f
        );
        
        // Random shard size - varying sizes for realism
        float sizeBase = 0.2f + (rand() % 100) / 100.0f * 1.0f;
        p.size = glm::vec2(sizeBase, sizeBase * (0.5f + (rand() % 100) / 100.0f));
        
        p.rotation = (rand() % 360) * 0.0174533f;
        p.rotSpeed = ((rand() % 1000) / 500.0f - 1.0f) * 15.0f;
        p.alpha = 1.0f;
        p.lifetime = 2.0f + (rand() % 100) / 50.0f;  // 2-4 seconds
        
        shatterParticles.push_back(p);
    }
    
    // Play break sound (console beep)
    std::cout << "\a" << std::flush;
}

// Forward declaration
static void ResetMirrors();

// Update shatter particles
static void UpdateShatter(float dt) {
    for (auto it = shatterParticles.begin(); it != shatterParticles.end(); ) {
        it->position += it->velocity * dt;
        it->velocity.y -= 12.0f * dt;  // Gravity
        it->rotation += it->rotSpeed * dt;
        it->lifetime -= dt;
        
        // Fade out as lifetime decreases
        if (it->lifetime < 1.0f) {
            it->alpha = it->lifetime;
        }
        
        if (it->lifetime <= 0.0f || it->alpha <= 0.0f) {
            it = shatterParticles.erase(it);
        } else {
            ++it;
        }
    }
}

// Check if character hits a mirror
static void CheckMirrorCollisions() {
    if (currentWorld != 4) return;  // Only in Hall of Mirrors
    
    float hitDist = 5.0f;  // Collision distance
    float cornerOffset = MIRROR_DISTANCE * 0.7f;  // Diagonal position
    
    // Front-left mirror (corner at -X, -Z)
    if (!mirrorBroken[0]) {
        float dx = characterPos.x - (-cornerOffset);
        float dz = characterPos.z - (-cornerOffset);
        if (sqrt(dx*dx + dz*dz) < hitDist && abs(characterPos.y) < MIRROR_HEIGHT/2) {
            BreakMirror(0, glm::vec3(-cornerOffset, 0, -cornerOffset), glm::vec3(1, 0, 1));
        }
    }
    
    // Front-right mirror (corner at +X, -Z)
    if (!mirrorBroken[1]) {
        float dx = characterPos.x - cornerOffset;
        float dz = characterPos.z - (-cornerOffset);
        if (sqrt(dx*dx + dz*dz) < hitDist && abs(characterPos.y) < MIRROR_HEIGHT/2) {
            BreakMirror(1, glm::vec3(cornerOffset, 0, -cornerOffset), glm::vec3(-1, 0, 1));
        }
    }
    
    // Back-left mirror (corner at -X, +Z)
    if (!mirrorBroken[2]) {
        float dx = characterPos.x - (-cornerOffset);
        float dz = characterPos.z - cornerOffset;
        if (sqrt(dx*dx + dz*dz) < hitDist && abs(characterPos.y) < MIRROR_HEIGHT/2) {
            BreakMirror(2, glm::vec3(-cornerOffset, 0, cornerOffset), glm::vec3(1, 0, -1));
        }
    }
    
    // Back-right mirror (corner at +X, +Z)
    if (!mirrorBroken[3]) {
        float dx = characterPos.x - cornerOffset;
        float dz = characterPos.z - cornerOffset;
        if (sqrt(dx*dx + dz*dz) < hitDist && abs(characterPos.y) < MIRROR_HEIGHT/2) {
            BreakMirror(3, glm::vec3(cornerOffset, 0, cornerOffset), glm::vec3(-1, 0, -1));
        }
    }
}

// Reset mirrors when entering hall of mirrors world
static void ResetMirrors() {
    for (int i = 0; i < 4; i++) {
        mirrorBroken[i] = false;
        mirrorBreakTime[i] = 0.0f;
    }
    shatterParticles.clear();
    worldExploding = false;
    worldExplosionTime = 0.0f;
}

static void UpdatePortal(float dt) {
    // Move all portals upward (character falls toward them)
    portalY += -characterVel.y * dt;
    portal2Y += -characterVel.y * dt;
    portal3Y += -characterVel.y * dt;
    portal4Y += -characterVel.y * dt;
    
    float distFromCenter = length(glm::vec2(characterPos.x, characterPos.z));
    
    // Check portal 1 (main - cycles through day/night) - front of diamond (-Z)
    // Expanded Y detection zone for more reliable entry
    if (timeInWorld > 7.0f && portalCooldown <= 0.0f && portalY > -4.0f && portalY < 4.0f) {
        float dist1 = length(glm::vec2(characterPos.x, characterPos.z + 15.0f));  // Front (-Z)
        if (dist1 < PORTAL_RADIUS * 0.85f) {
            if (currentWorld == 0) currentWorld = 1;
            else if (currentWorld == 1) currentWorld = 0;
            else if (currentWorld == 2) currentWorld = 0;
            else currentWorld = 0;  // Speedforce -> Day
            
            worldTransition = 1.0f;
            portalCooldown = 3.0f;
            timeInWorld = 0.0f;  // Reset time in world
            // Push portals far away - they need to travel ~7 seconds before becoming visible
            portalY = -150.0f;
            portal2Y = -180.0f;
            portal3Y = -210.0f;
            portal4Y = -240.0f;
            characterPos.x *= 0.5f;
            characterPos.z *= 0.5f;
        }
    }
    
    // Check portal 2 (mirror portal - right side of diamond) - only after 7 seconds in world
    if (timeInWorld > 7.0f && portalCooldown <= 0.0f && portal2Y > -4.0f && portal2Y < 4.0f) {
        float dist2 = length(glm::vec2(characterPos.x - 15.0f, characterPos.z));  // Right (+X)
        if (dist2 < 6.0f * 0.85f) {
            if (currentWorld == 0) currentWorld = 2;
            else if (currentWorld == 1) currentWorld = 2;
            else if (currentWorld == 2) currentWorld = 1;
            else currentWorld = 1;  // Speedforce -> Night
            
            worldTransition = 1.0f;
            portalCooldown = 3.0f;
            timeInWorld = 0.0f;  // Reset time in world
            portalY = -160.0f;
            portal2Y = -190.0f;
            portal3Y = -220.0f;
            portal4Y = -250.0f;
            characterPos.x *= 0.5f;
            characterPos.z *= 0.5f;
        }
    }
    
    // Check portal 3 (speedforce portal - left side of diamond) - only after 7 seconds in world
    if (timeInWorld > 7.0f && portalCooldown <= 0.0f && portal3Y > -4.0f && portal3Y < 4.0f) {
        float dist3 = length(glm::vec2(characterPos.x + 15.0f, characterPos.z));  // Left (-X)
        if (dist3 < 5.0f * 0.85f) {
            if (currentWorld == 3) currentWorld = 2;  // Speedforce -> Chrome
            else if (currentWorld == 4) currentWorld = 3;  // Hall -> Speedforce
            else currentWorld = 3;  // Any -> Speedforce
            
            worldTransition = 1.0f;
            portalCooldown = 3.0f;
            timeInWorld = 0.0f;  // Reset time in world
            portalY = -150.0f;
            portal2Y = -180.0f;
            portal3Y = -220.0f;
            portal4Y = -250.0f;
            characterPos.x *= 0.5f;
            characterPos.z *= 0.5f;
        }
    }
    
    // Check portal 4 (hall of mirrors portal - back of diamond) - only after 7 seconds in world
    if (timeInWorld > 7.0f && portalCooldown <= 0.0f && portal4Y > -4.0f && portal4Y < 4.0f) {
        float dist4 = length(glm::vec2(characterPos.x, characterPos.z - 15.0f));  // Back (+Z at z=15)
        if (dist4 < 5.0f * 0.85f) {
            if (currentWorld == 4) currentWorld = 0;  // Hall -> Day
            else {
                currentWorld = 4;  // Any -> Hall of Mirrors
                ResetMirrors();    // Reset all mirrors when entering
            }
            
            worldTransition = 1.0f;
            portalCooldown = 3.0f;
            timeInWorld = 0.0f;  // Reset time in world
            portalY = -150.0f;
            portal2Y = -180.0f;
            portal3Y = -220.0f;
            portal4Y = -250.0f;
            characterPos.x *= 0.3f;  // Center character more for mirrors
            characterPos.z *= 0.3f;
        }
    }
    
    // Reset portals if they go too far above (lower thresholds to keep portals visible)
    if (portalY > 80.0f) {
        portalY = -40.0f - (rand() / (float)RAND_MAX) * 20.0f;
    }
    if (portal2Y > 90.0f) {
        portal2Y = -50.0f - (rand() / (float)RAND_MAX) * 25.0f;
    }
    if (portal3Y > 100.0f) {
        portal3Y = -60.0f - (rand() / (float)RAND_MAX) * 30.0f;
    }
    if (portal4Y > 110.0f) {
        portal4Y = -70.0f - (rand() / (float)RAND_MAX) * 35.0f;
    }
    
    // Update cooldown and transition
    if (portalCooldown > 0.0f) portalCooldown -= dt;
    if (worldTransition > 0.0f) worldTransition -= dt * 2.0f;
    if (worldTransition < 0.0f) worldTransition = 0.0f;
    
    // Update time in current world (for delayed portal appearance)
    timeInWorld += dt;
    
    // Thunder sounds in speedforce world (lightning flashes disabled)
    if (currentWorld == 3) {
        lightningTimer -= dt;
        if (lightningTimer <= 0.0f) {
            // No more big flash - just reset timer for thunder
            lightningTimer = 0.3f + (rand() / (float)RAND_MAX) * 1.5f;  // Random interval
            thunderTimer = 0.1f + (rand() / (float)RAND_MAX) * 0.3f;  // Delay for thunder
        }
        
        // Thunder sound trigger (using system beep as placeholder)
        if (thunderTimer > 0.0f) {
            thunderTimer -= dt;
            if (thunderTimer <= 0.0f) {
                // Thunder! (Console beep - Windows)
                std::cout << "\a" << std::flush;  // System beep for thunder
            }
        }
        
        lightningFlash = 0.0f;  // Keep flash disabled
    } else {
        lightningFlash = 0.0f;
    }
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
static void key_callback(GLFWwindow*, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    
    if (key == GLFW_KEY_W) keyW = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_S) keyS = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_A) keyA = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_D) keyD = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_SPACE) keySpace = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_LEFT_SHIFT) keyShift = (action != GLFW_RELEASE);
    
    // Toggle GLTF bot model
    if (key == GLFW_KEY_T && action == GLFW_PRESS) {
        useGLTFBot = !useGLTFBot;
        std::cout << "Character model: " << (useGLTFBot ? "GLTF Bot" : "Simple Box") << std::endl;
    }
    
}

static void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    if (firstMouse) {
        lastMouseX = (float)xpos;
        lastMouseY = (float)ypos;
        firstMouse = false;
    }
    
    float xoffset = (float)xpos - lastMouseX;
    float yoffset = lastMouseY - (float)ypos; // Reversed: y goes bottom to top
    lastMouseX = (float)xpos;
    lastMouseY = (float)ypos;
    
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;
    
    cameraYaw += xoffset;
    cameraPitch += yoffset;
    
    // Clamp pitch to avoid flipping
    if (cameraPitch > 89.0f) cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;
}

static void scroll_callback(GLFWwindow*, double, double yoffset) {
    cameraDistance -= (float)yoffset * 2.0f;
    if (cameraDistance < 5.0f) cameraDistance = 5.0f;
    if (cameraDistance > 50.0f) cameraDistance = 50.0f;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    if (!glfwInit()) {
        std::cout << "GLFW init failed\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(windowWidth, windowHeight, "ENDLESS FREEFALL", nullptr, nullptr);
    if (!window) {
        std::cout << "Window creation failed\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (gladLoadGL(glfwGetProcAddress) == 0) {
        std::cout << "GLAD init failed\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Initialize everything
    InitSkybox();
    InitCharacter();
    InitBirdsAndAirplane();
    InitRingWarriors();
    InitSpeedsters();
    InitDBZFighters();
    InitParticles();
    InitPortal();
    InitMirrors();
    
    // Initialize GLTF bot
    bot.initialize();

    std::cout << "=== ENDLESS FREEFALL ===\n";
    std::cout << "Watch the character tumble through the infinite sky!\n";
    std::cout << "Fall through PORTALS to travel between worlds!\n";
    std::cout << "  - LARGE portal (purple/orange): Day <-> Space Verse\n";
    std::cout << "  - MEDIUM portal (silver): Chrome Dimension\n";
    std::cout << "  - SMALL portal (red/orange): SPEEDFORCE!\n";
    std::cout << "  - BACK portal (gold): HALL OF MIRRORS!\n";
    std::cout << "Controls:\n";
    std::cout << "  Mouse - Orbit camera around character\n";
    std::cout << "  Scroll - Zoom in/out\n";
    std::cout << "  WASD - Move character (can miss portals!)\n";
    std::cout << "  Space - Spread arms (slow fall)\n";
    std::cout << "  Shift - Dive (fast fall)\n";
    std::cout << "  T - Toggle character model (GLTF/Simple)\n";
    std::cout << "  ESC - Exit\n\n";

    double lastT = glfwGetTime();
    float totalFallDistance = 0.0f;
    
    // FPS tracking
    double fpsLastTime = glfwGetTime();
    int frameCount = 0;

    while (!glfwWindowShouldClose(window)) {
        double nowT = glfwGetTime();
        float dt = (float)(nowT - lastT);
        lastT = nowT;
        windTime += dt;
        
        // Update FPS counter
        frameCount++;
        if (nowT - fpsLastTime >= 1.0) {
            char title[128];
            const char* worldName;
            if (currentWorld == 0) worldName = "DAY WORLD";
            else if (currentWorld == 1) worldName = "SPACE VERSE";
            else if (currentWorld == 2) worldName = "CHROME DIMENSION";
            else if (currentWorld == 3) worldName = "SPEEDFORCE";
            else worldName = "HALL OF MIRRORS";
            snprintf(title, sizeof(title), "ENDLESS FREEFALL - %s - FPS: %d", worldName, frameCount);
            glfwSetWindowTitle(window, title);
            frameCount = 0;
            fpsLastTime = nowT;
        }

        // ---------------------------------------------------------------------
        // Update character physics
        // ---------------------------------------------------------------------
        
        // Base fall speed
        float baseFallSpeed = -80.0f;
        
        // Modify fall speed based on input
        if (keySpace) {
            // Spread arms - slow down
            baseFallSpeed = -40.0f;
            tumbleSpeedX *= 0.98f;  // Stabilize
            tumbleSpeedZ *= 0.98f;
        } else if (keyShift) {
            // Dive - speed up
            baseFallSpeed = -150.0f;
            tumbleSpeedX = glm::mix(tumbleSpeedX, 90.0f, dt * 2.0f);
        }
        
        // Lerp to target fall speed
        characterVel.y = glm::mix(characterVel.y, baseFallSpeed, dt * 3.0f);
        
        // Simple direct movement - WASD directly adds velocity
        float moveSpeed = 30.0f;
        if (keyW) characterVel.z -= moveSpeed * dt;
        if (keyS) characterVel.z += moveSpeed * dt;
        if (keyA) characterVel.x -= moveSpeed * dt;
        if (keyD) characterVel.x += moveSpeed * dt;
        
        // Dampen horizontal velocity
        characterVel.x *= 0.95f;
        characterVel.z *= 0.95f;
        
        // Update character position
        characterPos.x += characterVel.x * dt;
        characterPos.z += characterVel.z * dt;
        
        // Update fall distance (for sky scrolling)
        totalFallDistance += -characterVel.y * dt;
        
        // Update particles
        UpdateParticles(dt);
        
        // Update birds and airplane (day world only)
        UpdateBirdsAndAirplane(dt);
        
        // Update ring warriors battle (night world only)
        UpdateRingWarriors(dt);
        
        // Update speedsters race (speedforce world only)
        UpdateSpeedsters(dt);
        
        // Update DBZ fighters (chrome world only)
        UpdateDBZFighters(dt);
        
        // Update portal
        UpdatePortal(dt);
        
        // Update mirror collisions and shatter
        CheckMirrorCollisions();
        UpdateShatter(dt);

        // ---------------------------------------------------------------------
        // Camera (mouse-controlled orbit around character)
        // ---------------------------------------------------------------------
        float camX = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch)) * cameraDistance;
        float camY = sin(glm::radians(cameraPitch)) * cameraDistance + 2.0f;
        float camZ = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch)) * cameraDistance;
        
        // Camera follows character position - character always at center
        glm::vec3 cameraPos(camX + characterPos.x, camY, camZ + characterPos.z);
        glm::vec3 cameraTarget(characterPos.x, 0.0f, characterPos.z);  // Look at character center
        
        glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 
            (float)windowWidth / (float)windowHeight, 0.1f, 1000.0f);
        glm::mat4 vp = proj * view;

        // ---------------------------------------------------------------------
        // Render mirror reflections to framebuffers (only in Hall of Mirrors)
        // ---------------------------------------------------------------------
        if (currentWorld == 4 && !worldExploding) {
            glm::mat4 reflProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 500.0f);
            for (int i = 0; i < 4; i++) {
                RenderMirrorReflection(i, cameraPos, reflProj, skyboxProgram, skyboxVAO, 
                                       characterProgram, characterVAO, characterIndexCount,
                                       (float)nowT, totalFallDistance);
            }
            // Restore viewport
            glViewport(0, 0, windowWidth, windowHeight);
        }

        // ---------------------------------------------------------------------
        // Render
        // ---------------------------------------------------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw skybox
        glDepthFunc(GL_LEQUAL);
        glUseProgram(skyboxProgram);
        glm::mat4 skyView = glm::mat4(glm::mat3(view));  // Remove translation
        glm::mat4 skyVP = proj * skyView;
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "uVP"), 1, GL_FALSE, glm::value_ptr(skyVP));
        glUniform1f(glGetUniformLocation(skyboxProgram, "uTime"), (float)nowT);
        glUniform1f(glGetUniformLocation(skyboxProgram, "uFallDistance"), totalFallDistance);
        glUniform1i(glGetUniformLocation(skyboxProgram, "uWorldType"), currentWorld);
        glUniform1f(glGetUniformLocation(skyboxProgram, "uTransition"), worldTransition + lightningFlash);
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);

        // Detect movement direction for pose animation based on velocity
        // 0=idle/tumble, 1=north(forward), 2=left, 3=right, 4=south(backward), 5=north-west, 6=north-east
        int moveDir = 0;
        float moveBlend = 1.0f;  // Full blend to target pose
        
        // Use velocity direction for pose detection (smoother transitions)
        float horizSpeed = sqrt(characterVel.x * characterVel.x + characterVel.z * characterVel.z);
        if (horizSpeed > 1.0f) {
            // Determine primary movement direction from velocity
            float absX = fabs(characterVel.x);
            float absZ = fabs(characterVel.z);
            
            // Check for diagonal north movement first
            if (characterVel.z < 0 && absX > 0.3f && absZ > 0.3f) {
                // Moving north with significant horizontal component
                if (characterVel.x < 0) {
                    moveDir = 5;  // North-west - Superman pose
                } else {
                    moveDir = 6;  // North-east - Superman pose
                }
            } else if (characterVel.z < 0 && absZ > absX * 0.5f) {
                moveDir = 1;  // North (negative Z) - Superman pose
            } else if (characterVel.z > 0 && absZ > absX * 0.5f) {
                moveDir = 4;  // South (positive Z) - floating on back
            } else if (characterVel.x < 0 && absX > absZ * 0.5f) {
                moveDir = 2;  // Left (negative X) - gliding pose
            } else if (characterVel.x > 0 && absX > absZ * 0.5f) {
                moveDir = 3;  // Right (positive X) - gliding pose
            } else if (characterVel.z < 0) {
                moveDir = 1;  // Default to north if moving forward-ish
            } else {
                moveDir = 4;  // Default to south if moving backward-ish
            }
            
            // Blend based on how fast we're moving
            moveBlend = glm::clamp(horizSpeed / 20.0f, 0.0f, 1.0f);
        }
        
        // Update bot pose based on movement direction
        bot.updateMovementPose((float)nowT, moveDir, moveBlend);

        glm::vec3 charRenderPos(characterPos.x, 0.0f, characterPos.z);

        // Draw character at its actual position
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, charRenderPos);
        // Always apply tumbling animation
        model = glm::rotate(model, glm::radians(characterRotX), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(characterRotZ), glm::vec3(0, 0, 1));
        
        if (useGLTFBot) {
            // Use animated GLTF bot model
            // Rotate 180 degrees around Y to face north
            model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0, 1, 0));
            // Rotate 90 degrees around X to lay flat (diving pose)
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
            // Add forward tilt (leaning into the dive)
            model = glm::rotate(model, glm::radians(15.0f), glm::vec3(1, 0, 0));
            // Tilt based on movement direction (lean into movement)
            float tiltAmount = 25.0f;  // Max tilt angle in degrees
            float tiltX = glm::clamp(characterVel.z * 0.5f, -tiltAmount, tiltAmount);  // Forward/back tilt
            float tiltZ = glm::clamp(characterVel.x * 0.5f, -tiltAmount, tiltAmount);  // Left/right tilt (swapped)
            model = glm::rotate(model, glm::radians(tiltX), glm::vec3(1, 0, 0));
            model = glm::rotate(model, glm::radians(tiltZ), glm::vec3(0, 0, 1));
            model = glm::scale(model, glm::vec3(0.04f));  // GLTF model is much larger
            // Center the model - the GLTF has a root offset of ~(-280, -105, 0)
            model = glm::translate(model, glm::vec3(280.0f, 105.0f, 0.0f));  // Counter the root offset
            glm::mat4 mvp = proj * view * model;
            
            // Light position based on world
            glm::vec3 lightPos;
            glm::vec3 lightInt(5e6f, 5e6f, 5e6f);
            if (currentWorld == 0) {
                lightPos = charRenderPos + glm::vec3(100.0f, 200.0f, 100.0f);
            } else if (currentWorld == 1) {
                lightPos = charRenderPos + glm::vec3(200.0f, 100.0f, 0.0f);
                lightInt = glm::vec3(3e6f, 3e6f, 4e6f);
            } else if (currentWorld == 2) {
                lightPos = charRenderPos + glm::vec3(0.0f, 150.0f, 0.0f);
                lightInt = glm::vec3(6e6f, 6e6f, 6e6f);
            } else if (currentWorld == 3) {
                float flicker = 1.0f + lightningFlash * 0.5f;
                lightPos = charRenderPos + glm::vec3(50.0f * flicker, 100.0f, 50.0f);
                lightInt = glm::vec3(5e6f * flicker, 4e6f * flicker, 2e6f);
            } else {
                lightPos = charRenderPos + glm::vec3(0.0f, 150.0f, 0.0f);
                lightInt = glm::vec3(6e6f, 5e6f, 3e6f);
            }
            
            bot.render(mvp, model, cameraPos, lightPos, lightInt, currentWorld);
        } else {
            // Use animated simple box character
            glUseProgram(characterProgram);
            
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
            
            // Different lighting for each world
            if (currentWorld == 0) {
                glUniform3f(glGetUniformLocation(characterProgram, "uLightDir"), 0.5f, 0.8f, 0.3f);
            } else if (currentWorld == 1) {
                glUniform3f(glGetUniformLocation(characterProgram, "uLightDir"), 0.8f, -0.2f, 0.5f);
            } else if (currentWorld == 2) {
                glUniform3f(glGetUniformLocation(characterProgram, "uLightDir"), 0.0f, 1.0f, 0.0f);
            } else if (currentWorld == 3) {
                float flicker = 0.5f + lightningFlash * 0.5f;
                glUniform3f(glGetUniformLocation(characterProgram, "uLightDir"), flicker, 0.3f + flicker * 0.5f, 0.1f);
            } else {
                glUniform3f(glGetUniformLocation(characterProgram, "uLightDir"), 0.3f, 0.9f, 0.2f);
            }
            glUniform1f(glGetUniformLocation(characterProgram, "uReflection"), 0.0f);
            glUniform3fv(glGetUniformLocation(characterProgram, "uViewPos"), 1, glm::value_ptr(cameraPos));
            
            // Determine TARGET pose based on velocity
            float horizSpeed = sqrt(characterVel.x * characterVel.x + characterVel.z * characterVel.z);
            
            // Target animation angles (flipped 180 - head points down towards ground)
            float targetBodyTilt = -90.0f;   // Default: diving head-down (head towards ground)
            float targetArmSwing = 0.0f;
            float targetArmSpread = 30.0f;   // Arms slightly spread when diving
            float targetLegSwing = 0.0f;
            float targetLegSpread = 15.0f;   // Legs slightly apart when diving
            
            if (horizSpeed > 0.5f) {
                // Determine direction
                float absX = fabs(characterVel.x);
                float absZ = fabs(characterVel.z);
                
                if (characterVel.z < 0 && absZ > absX * 0.3f) {
                    // Moving forward - Superman flying pose (horizontal)
                    targetBodyTilt = -90.0f;     // Horizontal, head forward
                    targetArmSwing = -160.0f;    // Arms stretched forward (above head when diving)
                    targetArmSpread = 10.0f;     // Arms close together
                    targetLegSwing = 0.0f;       // Legs straight back
                    targetLegSpread = 5.0f;      // Legs together
                } else if (characterVel.z > 0 && absZ > absX * 0.3f) {
                    // Moving backward - floating on back
                    targetBodyTilt = 90.0f;      // Facing up (back down)
                    targetArmSpread = 80.0f;     // Arms spread out to sides
                    targetArmSwing = 0.0f;
                    targetLegSwing = 0.0f;
                    targetLegSpread = 30.0f;     // Legs relaxed apart
                } else if (characterVel.x < 0) {
                    // Moving left - side glide
                    targetBodyTilt = -70.0f;
                    targetArmSwing = -120.0f;    // Arms forward
                    targetArmSpread = 20.0f;
                    targetLegSpread = 10.0f;
                } else {
                    // Moving right - side glide
                    targetBodyTilt = -70.0f;
                    targetArmSwing = -120.0f;    // Arms forward
                    targetArmSpread = 20.0f;
                    targetLegSpread = 10.0f;
                }
            }
            
            // Smooth interpolation (lerp) towards target pose
            float smoothSpeed = 5.0f * dt;  // Adjust for faster/slower transitions
            currentBodyTilt = glm::mix(currentBodyTilt, targetBodyTilt, smoothSpeed);
            currentArmSwing = glm::mix(currentArmSwing, targetArmSwing, smoothSpeed);
            currentArmSpread = glm::mix(currentArmSpread, targetArmSpread, smoothSpeed);
            currentLegSwing = glm::mix(currentLegSwing, targetLegSwing, smoothSpeed);
            currentLegSpread = glm::mix(currentLegSpread, targetLegSpread, smoothSpeed);
            
            // Base scale
            float scale = 1.5f;
            
            // Part 0: Torso - rotated to face down (diving position)
            glm::mat4 torsoMat = model;
            torsoMat = glm::scale(torsoMat, glm::vec3(scale));
            torsoMat = glm::rotate(torsoMat, glm::radians(currentBodyTilt), glm::vec3(1, 0, 0));
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(torsoMat));
            glBindVertexArray(bodyPartVAOs[0]);
            glDrawElements(GL_TRIANGLES, bodyPartIndexCounts[0], GL_UNSIGNED_INT, 0);
            
            // Part 1: Head (attached to torso, now at bottom when diving)
            glm::mat4 headMat = torsoMat;
            headMat = glm::translate(headMat, glm::vec3(0.0f, 0.85f, 0.0f));
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(headMat));
            glBindVertexArray(bodyPartVAOs[1]);
            glDrawElements(GL_TRIANGLES, bodyPartIndexCounts[1], GL_UNSIGNED_INT, 0);
            
            // Part 2: Left arm (pivot at shoulder)
            glm::mat4 leftArmMat = torsoMat;
            leftArmMat = glm::translate(leftArmMat, glm::vec3(-0.55f, 0.45f, 0.0f));  // Shoulder position
            leftArmMat = glm::rotate(leftArmMat, glm::radians(currentArmSwing), glm::vec3(1, 0, 0));
            leftArmMat = glm::rotate(leftArmMat, glm::radians(-currentArmSpread), glm::vec3(0, 0, 1));
            leftArmMat = glm::translate(leftArmMat, glm::vec3(0.0f, -0.4f, 0.0f));
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(leftArmMat));
            glBindVertexArray(bodyPartVAOs[2]);
            glDrawElements(GL_TRIANGLES, bodyPartIndexCounts[2], GL_UNSIGNED_INT, 0);
            
            // Part 3: Right arm (pivot at shoulder)
            glm::mat4 rightArmMat = torsoMat;
            rightArmMat = glm::translate(rightArmMat, glm::vec3(0.55f, 0.45f, 0.0f));  // Shoulder position
            rightArmMat = glm::rotate(rightArmMat, glm::radians(currentArmSwing), glm::vec3(1, 0, 0));
            rightArmMat = glm::rotate(rightArmMat, glm::radians(currentArmSpread), glm::vec3(0, 0, 1));
            rightArmMat = glm::translate(rightArmMat, glm::vec3(0.0f, -0.4f, 0.0f));
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(rightArmMat));
            glBindVertexArray(bodyPartVAOs[3]);
            glDrawElements(GL_TRIANGLES, bodyPartIndexCounts[3], GL_UNSIGNED_INT, 0);
            
            // Part 4: Left leg (pivot at hip)
            glm::mat4 leftLegMat = torsoMat;
            leftLegMat = glm::translate(leftLegMat, glm::vec3(-0.2f, -0.6f, 0.0f));  // Hip position
            leftLegMat = glm::rotate(leftLegMat, glm::radians(currentLegSwing), glm::vec3(1, 0, 0));
            leftLegMat = glm::rotate(leftLegMat, glm::radians(currentLegSpread), glm::vec3(0, 0, 1));
            leftLegMat = glm::translate(leftLegMat, glm::vec3(0.0f, -0.5f, 0.0f));
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(leftLegMat));
            glBindVertexArray(bodyPartVAOs[4]);
            glDrawElements(GL_TRIANGLES, bodyPartIndexCounts[4], GL_UNSIGNED_INT, 0);
            
            // Part 5: Right leg (pivot at hip)
            glm::mat4 rightLegMat = torsoMat;
            rightLegMat = glm::translate(rightLegMat, glm::vec3(0.2f, -0.6f, 0.0f));  // Hip position
            rightLegMat = glm::rotate(rightLegMat, glm::radians(currentLegSwing), glm::vec3(1, 0, 0));
            rightLegMat = glm::rotate(rightLegMat, glm::radians(-currentLegSpread), glm::vec3(0, 0, 1));
            rightLegMat = glm::translate(rightLegMat, glm::vec3(0.0f, -0.5f, 0.0f));
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(rightLegMat));
            glBindVertexArray(bodyPartVAOs[5]);
            glDrawElements(GL_TRIANGLES, bodyPartIndexCounts[5], GL_UNSIGNED_INT, 0);
        }
        
        // =====================================================================
        // Draw Birds and Airplane (day world only)
        // =====================================================================
        if (currentWorld == 0) {
            // Use the character shader for birds and airplane
            glUseProgram(characterProgram);
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform3fv(glGetUniformLocation(characterProgram, "uLightDir"), 1, glm::value_ptr(glm::vec3(0.5f, 0.8f, 0.3f)));
            glUniform3fv(glGetUniformLocation(characterProgram, "uViewPos"), 1, glm::value_ptr(cameraPos));
            glUniform1f(glGetUniformLocation(characterProgram, "uReflection"), 0.0f);
            glUniform1i(glGetUniformLocation(characterProgram, "uMirrorClipMode"), 0);
            
            // Draw each bird
            for (int i = 0; i < NUM_BIRDS; i++) {
                glm::vec3 birdWorldPos = characterPos + birdBots[i].position;
                
                // Calculate bird rotation to face movement direction
                float birdYaw = atan2(birdBots[i].velocity.x, -birdBots[i].velocity.z);
                
                // Bird body
                glm::mat4 birdModel = glm::mat4(1.0f);
                birdModel = glm::translate(birdModel, birdWorldPos);
                birdModel = glm::rotate(birdModel, birdYaw, glm::vec3(0, 1, 0));
                birdModel = glm::scale(birdModel, glm::vec3(birdBots[i].size));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(birdModel));
                glBindVertexArray(birdBodyVAO);
                glDrawElements(GL_TRIANGLES, birdBodyIndexCount, GL_UNSIGNED_INT, 0);
                
                // Wing flap angle
                float wingFlap = sin(birdBots[i].wingPhase) * 0.7f;  // +/- 40 degrees
                
                // Left wing
                glm::mat4 leftWingMat = birdModel;
                leftWingMat = glm::translate(leftWingMat, glm::vec3(-0.15f, 0.05f, 0.0f));
                leftWingMat = glm::rotate(leftWingMat, wingFlap, glm::vec3(0, 0, 1));
                leftWingMat = glm::scale(leftWingMat, glm::vec3(-1, 1, 1));  // Mirror for left wing
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(leftWingMat));
                glBindVertexArray(birdWingVAO);
                glDrawElements(GL_TRIANGLES, birdWingIndexCount, GL_UNSIGNED_INT, 0);
                
                // Right wing
                glm::mat4 rightWingMat = birdModel;
                rightWingMat = glm::translate(rightWingMat, glm::vec3(0.15f, 0.05f, 0.0f));
                rightWingMat = glm::rotate(rightWingMat, -wingFlap, glm::vec3(0, 0, 1));
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(rightWingMat));
                glDrawElements(GL_TRIANGLES, birdWingIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // Draw airplane if active
            if (airplane.active) {
                glm::vec3 planeWorldPos = characterPos + airplane.position;
                
                // Calculate airplane rotation to face movement direction
                float planeYaw = atan2(airplane.direction.x, -airplane.direction.z);
                
                // Airplane scale
                float planeScale = 2.0f;
                
                // Airplane body
                glm::mat4 planeModel = glm::mat4(1.0f);
                planeModel = glm::translate(planeModel, planeWorldPos);
                planeModel = glm::rotate(planeModel, planeYaw, glm::vec3(0, 1, 0));
                planeModel = glm::scale(planeModel, glm::vec3(planeScale));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(planeModel));
                glBindVertexArray(airplaneBodyVAO);
                glDrawElements(GL_TRIANGLES, airplaneBodyIndexCount, GL_UNSIGNED_INT, 0);
                
                // Airplane wings (attached to body)
                glm::mat4 wingModel = planeModel;
                wingModel = glm::translate(wingModel, glm::vec3(0.0f, 0.0f, 0.5f));  // Slightly forward
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(wingModel));
                glBindVertexArray(airplaneWingVAO);
                glDrawElements(GL_TRIANGLES, airplaneWingIndexCount, GL_UNSIGNED_INT, 0);
                
                // Airplane tail
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(planeModel));
                glBindVertexArray(airplaneTailVAO);
                glDrawElements(GL_TRIANGLES, airplaneTailIndexCount, GL_UNSIGNED_INT, 0);
            }
        }
        
        // =====================================================================
        // Draw Ring Warriors Battle (night world only)
        // =====================================================================
        if (currentWorld == 1) {
            glUseProgram(characterProgram);
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform3fv(glGetUniformLocation(characterProgram, "uLightDir"), 1, glm::value_ptr(glm::vec3(0.3f, 0.8f, 0.5f)));
            glUniform3fv(glGetUniformLocation(characterProgram, "uViewPos"), 1, glm::value_ptr(cameraPos));
            glUniform1f(glGetUniformLocation(characterProgram, "uReflection"), 0.0f);
            glUniform1i(glGetUniformLocation(characterProgram, "uMirrorClipMode"), 0);
            
            // Draw Green Hero
            glm::vec3 heroWorldPos = characterPos + greenHero.position;
            glm::mat4 heroModel = glm::mat4(1.0f);
            heroModel = glm::translate(heroModel, heroWorldPos);
            heroModel = glm::rotate(heroModel, greenHero.bodyRotation, glm::vec3(0, 1, 0));
            heroModel = glm::scale(heroModel, glm::vec3(2.0f));
            
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(heroModel));
            glBindVertexArray(heroBodyVAO);
            glDrawElements(GL_TRIANGLES, heroBodyIndexCount, GL_UNSIGNED_INT, 0);
            
            // Draw Green Hero's extended arm during fighting (one arm out - right arm with ring)
            if (battleState == 0) {
                glm::mat4 heroArmModel = glm::mat4(1.0f);
                heroArmModel = glm::translate(heroArmModel, heroWorldPos);
                heroArmModel = glm::rotate(heroArmModel, greenHero.bodyRotation, glm::vec3(0, 1, 0));
                heroArmModel = glm::translate(heroArmModel, glm::vec3(0.6f, 2.3f, 0.0f));  // Right shoulder position (scaled)
                heroArmModel = glm::rotate(heroArmModel, glm::radians(-90.0f), glm::vec3(1, 0, 0));  // Point arm forward
                heroArmModel = glm::scale(heroArmModel, glm::vec3(2.0f));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(heroArmModel));
                glBindVertexArray(heroArmVAO);
                glDrawElements(GL_TRIANGLES, heroArmIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // Draw Yellow Villain
            glm::vec3 villainWorldPos = characterPos + yellowVillain.position;
            glm::mat4 villainModel = glm::mat4(1.0f);
            villainModel = glm::translate(villainModel, villainWorldPos);
            villainModel = glm::rotate(villainModel, yellowVillain.bodyRotation, glm::vec3(0, 1, 0));
            villainModel = glm::scale(villainModel, glm::vec3(2.0f));
            
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(villainModel));
            glBindVertexArray(villainBodyVAO);
            glDrawElements(GL_TRIANGLES, villainBodyIndexCount, GL_UNSIGNED_INT, 0);
            
            // Draw Yellow Villain's extended arm during fighting (one arm out - right arm with ring)
            if (battleState == 0) {
                glm::mat4 villainArmModel = glm::mat4(1.0f);
                villainArmModel = glm::translate(villainArmModel, villainWorldPos);
                villainArmModel = glm::rotate(villainArmModel, yellowVillain.bodyRotation, glm::vec3(0, 1, 0));
                villainArmModel = glm::translate(villainArmModel, glm::vec3(0.6f, 2.3f, 0.0f));  // Right shoulder position (scaled)
                villainArmModel = glm::rotate(villainArmModel, glm::radians(-90.0f), glm::vec3(1, 0, 0));  // Point arm forward
                villainArmModel = glm::scale(villainArmModel, glm::vec3(2.0f));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(villainArmModel));
                glBindVertexArray(villainArmVAO);
                glDrawElements(GL_TRIANGLES, villainArmIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            glm::vec3 collisionWorldPos = characterPos + energyCollision.position;
            
            // Only draw beams and energy ball during FIGHTING state (battleState == 0)
            if (battleState == 0) {
                // Draw GREEN beam from hero to collision (series of green spheres)
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending
                
                glm::vec3 greenStart = heroWorldPos + glm::vec3(1.0f, 0.5f, 0.0f);
                glm::vec3 greenDir = collisionWorldPos - greenStart;
                float greenDist = glm::length(greenDir);
                int greenSegments = (int)(greenDist / 1.5f);
                for (int i = 0; i < greenSegments; i++) {
                    float t = (float)i / greenSegments;
                    glm::vec3 beamPos = greenStart + greenDir * t;
                    float beamSize = 0.4f + sin((float)glfwGetTime() * 10.0f + t * 5.0f) * 0.1f;
                    
                    glm::mat4 beamModel = glm::mat4(1.0f);
                    beamModel = glm::translate(beamModel, beamPos);
                    beamModel = glm::scale(beamModel, glm::vec3(beamSize));
                    
                    glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(beamModel));
                    glBindVertexArray(greenBallVAO);
                    glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
                }
                
                // Draw YELLOW beam from villain to collision
                glm::vec3 yellowStart = villainWorldPos + glm::vec3(-1.0f, 0.5f, 0.0f);
                glm::vec3 yellowDir = collisionWorldPos - yellowStart;
                float yellowDist = glm::length(yellowDir);
                int yellowSegments = (int)(yellowDist / 1.5f);
                for (int i = 0; i < yellowSegments; i++) {
                    float t = (float)i / yellowSegments;
                    glm::vec3 beamPos = yellowStart + yellowDir * t;
                    float beamSize = 0.4f + sin((float)glfwGetTime() * 10.0f + t * 5.0f + 1.57f) * 0.1f;
                    
                    glm::mat4 beamModel = glm::mat4(1.0f);
                    beamModel = glm::translate(beamModel, beamPos);
                    beamModel = glm::scale(beamModel, glm::vec3(beamSize));
                    
                    glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(beamModel));
                    glBindVertexArray(yellowBallVAO);
                    glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
                }
                
                // Draw MIXED Energy Ball at collision point (green-yellow lime color)
                if (energyCollision.radius > 0.1f) {
                    glm::mat4 ballModel = glm::mat4(1.0f);
                    ballModel = glm::translate(ballModel, collisionWorldPos);
                    
                    // Pulsing effect
                    float pulse = 1.0f + sin((float)glfwGetTime() * 10.0f) * 0.15f;
                    ballModel = glm::scale(ballModel, glm::vec3(energyCollision.radius * pulse));
                    
                    glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(ballModel));
                    glBindVertexArray(mixedBallVAO);  // Lime green (green+yellow mix)
                    glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
                    
                    // Draw inner bright white core
                    glm::mat4 coreModel = glm::mat4(1.0f);
                    coreModel = glm::translate(coreModel, collisionWorldPos);
                    coreModel = glm::scale(coreModel, glm::vec3(energyCollision.radius * 0.4f * pulse));
                    glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(coreModel));
                    glBindVertexArray(energyBallVAO);  // White core
                    glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
                }
            }
            
            // Draw explosion effect during EXPLODING state (battleState == 1)
            if (battleState == 1 && energyCollision.exploding && energyCollision.radius > 0.1f) {
                glm::mat4 ballModel = glm::mat4(1.0f);
                ballModel = glm::translate(ballModel, collisionWorldPos);
                
                // Pulsing effect
                float pulse = 1.0f + sin((float)glfwGetTime() * 10.0f) * 0.15f;
                float explosionScale = energyCollision.exploding ? 
                    (1.0f + energyCollision.explosionTimer * 3.0f) : 1.0f;
                ballModel = glm::scale(ballModel, glm::vec3(energyCollision.radius * pulse * explosionScale));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(ballModel));
                glBindVertexArray(mixedBallVAO);  // Lime green (green+yellow mix)
                glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
                
                // Draw inner bright white core
                glm::mat4 coreModel = glm::mat4(1.0f);
                coreModel = glm::translate(coreModel, collisionWorldPos);
                coreModel = glm::scale(coreModel, glm::vec3(energyCollision.radius * 0.4f * pulse * explosionScale));
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(coreModel));
                glBindVertexArray(energyBallVAO);  // White core
                glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // Draw shockwave ring during explosion (mixed color)
            if (energyCollision.exploding && energyCollision.shockwaveRadius > 0.0f) {
                // Draw expanding ring
                glm::mat4 waveModel = glm::mat4(1.0f);
                waveModel = glm::translate(waveModel, collisionWorldPos);
                waveModel = glm::scale(waveModel, glm::vec3(energyCollision.shockwaveRadius, 0.3f, energyCollision.shockwaveRadius));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(waveModel));
                glBindVertexArray(mixedBallVAO);
                glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // Draw beam particles with correct colors
            for (size_t i = 0; i < beamParticles.size(); i++) {
                glm::vec3 particleWorldPos = characterPos + beamParticles[i].position;
                glm::mat4 particleModel = glm::mat4(1.0f);
                particleModel = glm::translate(particleModel, particleWorldPos);
                particleModel = glm::scale(particleModel, glm::vec3(beamParticles[i].size * beamParticles[i].life));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(particleModel));
                // Use correct color based on which warrior spawned it
                if (beamParticles[i].isGreen) {
                    glBindVertexArray(greenBallVAO);
                } else {
                    glBindVertexArray(yellowBallVAO);
                }
                glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        
        // =====================================================================
        // Draw Flash Racing (speedforce world only)
        // =====================================================================
        if (currentWorld == 3) {
            glUseProgram(characterProgram);
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform3fv(glGetUniformLocation(characterProgram, "uLightDir"), 1, glm::value_ptr(glm::vec3(0.5f, 0.8f, 0.3f)));
            glUniform3fv(glGetUniformLocation(characterProgram, "uViewPos"), 1, glm::value_ptr(cameraPos));
            glUniform1f(glGetUniformLocation(characterProgram, "uReflection"), 0.0f);
            glUniform1i(glGetUniformLocation(characterProgram, "uMirrorClipMode"), 0);
            
            // Draw The Flash (red)
            glm::vec3 flashWorldPos = characterPos + theFlash.position;
            glm::mat4 flashModel = glm::mat4(1.0f);
            flashModel = glm::translate(flashModel, flashWorldPos);
            flashModel = glm::rotate(flashModel, theFlash.bodyRotation, glm::vec3(0, 1, 0));
            // Add running lean
            flashModel = glm::rotate(flashModel, glm::radians(15.0f), glm::vec3(1, 0, 0));
            flashModel = glm::scale(flashModel, glm::vec3(2.5f));
            
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(flashModel));
            glBindVertexArray(flashBodyVAO);
            glDrawElements(GL_TRIANGLES, flashBodyIndexCount, GL_UNSIGNED_INT, 0);
            
            // Draw Reverse Flash (yellow)
            glm::vec3 reverseWorldPos = characterPos + reverseFlash.position;
            glm::mat4 reverseModel = glm::mat4(1.0f);
            reverseModel = glm::translate(reverseModel, reverseWorldPos);
            reverseModel = glm::rotate(reverseModel, reverseFlash.bodyRotation, glm::vec3(0, 1, 0));
            // Add running lean
            reverseModel = glm::rotate(reverseModel, glm::radians(15.0f), glm::vec3(1, 0, 0));
            reverseModel = glm::scale(reverseModel, glm::vec3(2.5f));
            
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(reverseModel));
            glBindVertexArray(reverseFlashBodyVAO);
            glDrawElements(GL_TRIANGLES, reverseFlashBodyIndexCount, GL_UNSIGNED_INT, 0);
            
            // Draw lightning trails with additive blending
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for glowing effect
            
            for (size_t i = 0; i < lightningTrails.size(); i++) {
                glm::vec3 trailWorldPos = characterPos + lightningTrails[i].position;
                glm::mat4 trailModel = glm::mat4(1.0f);
                trailModel = glm::translate(trailModel, trailWorldPos);
                
                // Random rotation for variety
                trailModel = glm::rotate(trailModel, lightningTrails[i].life * 10.0f, glm::vec3(0, 0, 1));
                trailModel = glm::scale(trailModel, glm::vec3(lightningTrails[i].size * lightningTrails[i].life * 2.0f));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(trailModel));
                
                // Use appropriate colored energy ball for trail
                if (lightningTrails[i].isRed) {
                    // Flash - red/yellow trail - use existing energy ball with red tint
                    glBindVertexArray(energyBallVAO);  // White/bright for lightning
                } else {
                    // Reverse Flash - yellow trail
                    glBindVertexArray(yellowBallVAO);
                }
                glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // Draw speed lines around the track (motion blur effect)
            float time = (float)glfwGetTime();
            for (int i = 0; i < 20; i++) {
                float angle = (float)i / 20.0f * 6.28318f + time * 2.0f;
                float radius = raceTrackRadius + sin(angle * 3.0f + time) * 5.0f;
                
                glm::vec3 linePos = characterPos + glm::vec3(
                    cos(angle) * radius,
                    3.0f + sin(angle * 5.0f) * 2.0f,
                    sin(angle) * radius
                );
                
                glm::mat4 lineModel = glm::mat4(1.0f);
                lineModel = glm::translate(lineModel, linePos);
                lineModel = glm::rotate(lineModel, angle, glm::vec3(0, 1, 0));
                lineModel = glm::scale(lineModel, glm::vec3(0.2f, 0.1f, 3.0f));  // Elongated for speed effect
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(lineModel));
                glBindVertexArray(energyBallVAO);
                glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        
        // =====================================================================
        // Draw Goku vs Cell Teleport Punch Battle (chrome world only)
        // =====================================================================
        if (currentWorld == 2) {
            glUseProgram(characterProgram);
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform3fv(glGetUniformLocation(characterProgram, "uLightDir"), 1, glm::value_ptr(glm::vec3(0.5f, 0.8f, 0.3f)));
            glUniform3fv(glGetUniformLocation(characterProgram, "uViewPos"), 1, glm::value_ptr(cameraPos));
            glUniform1f(glGetUniformLocation(characterProgram, "uReflection"), 0.3f);  // Chrome world reflection
            glUniform1i(glGetUniformLocation(characterProgram, "uMirrorClipMode"), 0);
            
            // Draw teleport afterimage trails (ghostly fading copies)
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            
            for (size_t i = 0; i < teleportTrails.size(); i++) {
                glm::vec3 trailWorldPos = characterPos + teleportTrails[i].position;
                glm::mat4 trailModel = glm::mat4(1.0f);
                trailModel = glm::translate(trailModel, trailWorldPos);
                trailModel = glm::rotate(trailModel, teleportTrails[i].rotation, glm::vec3(0, 1, 0));
                float trailScale = (teleportTrails[i].isGoku ? 3.0f : 3.5f) * (0.5f + teleportTrails[i].life);
                trailModel = glm::scale(trailModel, glm::vec3(trailScale));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(trailModel));
                if (teleportTrails[i].isGoku) {
                    glBindVertexArray(gokuBodyVAO);
                    glDrawElements(GL_TRIANGLES, gokuBodyIndexCount, GL_UNSIGNED_INT, 0);
                } else {
                    glBindVertexArray(cellBodyVAO);
                    glDrawElements(GL_TRIANGLES, cellBodyIndexCount, GL_UNSIGNED_INT, 0);
                }
            }
            
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            // Draw Goku
            glm::vec3 gokuWorldPos = characterPos + goku.position;
            glm::mat4 gokuModel = glm::mat4(1.0f);
            gokuModel = glm::translate(gokuModel, gokuWorldPos);
            gokuModel = glm::rotate(gokuModel, goku.bodyRotation, glm::vec3(0, 1, 0));
            
            // Hit recoil animation
            if (goku.isHit && goku.hitRecoil > 0) {
                gokuModel = glm::rotate(gokuModel, glm::radians(-20.0f * goku.hitRecoil), glm::vec3(1, 0, 0));
            }
            // Punch forward lean
            else if (goku.isPunching) {
                gokuModel = glm::rotate(gokuModel, glm::radians(30.0f), glm::vec3(1, 0, 0));
            }
            gokuModel = glm::scale(gokuModel, glm::vec3(3.0f));
            
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(gokuModel));
            glBindVertexArray(gokuBodyVAO);
            glDrawElements(GL_TRIANGLES, gokuBodyIndexCount, GL_UNSIGNED_INT, 0);
            
            // Draw Goku's punching arm when punching
            if (goku.isPunching) {
                glm::mat4 gokuArmModel = glm::mat4(1.0f);
                gokuArmModel = glm::translate(gokuArmModel, gokuWorldPos);
                gokuArmModel = glm::rotate(gokuArmModel, goku.bodyRotation, glm::vec3(0, 1, 0));
                gokuArmModel = glm::rotate(gokuArmModel, glm::radians(30.0f), glm::vec3(1, 0, 0));  // Punch lean
                gokuArmModel = glm::translate(gokuArmModel, glm::vec3(0.0f, 3.8f, 0.0f));
                gokuArmModel = glm::rotate(gokuArmModel, glm::radians(-90.0f), glm::vec3(1, 0, 0));
                gokuArmModel = glm::scale(gokuArmModel, glm::vec3(3.0f));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(gokuArmModel));
                glBindVertexArray(gokuArmsVAO);
                glDrawElements(GL_TRIANGLES, gokuArmsIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // Draw Cell
            glm::vec3 cellWorldPos = characterPos + cell.position;
            glm::mat4 cellModel = glm::mat4(1.0f);
            cellModel = glm::translate(cellModel, cellWorldPos);
            cellModel = glm::rotate(cellModel, cell.bodyRotation, glm::vec3(0, 1, 0));
            
            // Hit recoil animation
            if (cell.isHit && cell.hitRecoil > 0) {
                cellModel = glm::rotate(cellModel, glm::radians(-20.0f * cell.hitRecoil), glm::vec3(1, 0, 0));
            }
            // Punch forward lean
            else if (cell.isPunching) {
                cellModel = glm::rotate(cellModel, glm::radians(30.0f), glm::vec3(1, 0, 0));
            }
            cellModel = glm::scale(cellModel, glm::vec3(3.5f));
            
            glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(cellModel));
            glBindVertexArray(cellBodyVAO);
            glDrawElements(GL_TRIANGLES, cellBodyIndexCount, GL_UNSIGNED_INT, 0);
            
            // Draw Cell's punching arm when punching
            if (cell.isPunching) {
                glm::mat4 cellArmModel = glm::mat4(1.0f);
                cellArmModel = glm::translate(cellArmModel, cellWorldPos);
                cellArmModel = glm::rotate(cellArmModel, cell.bodyRotation, glm::vec3(0, 1, 0));
                cellArmModel = glm::rotate(cellArmModel, glm::radians(30.0f), glm::vec3(1, 0, 0));  // Punch lean
                cellArmModel = glm::translate(cellArmModel, glm::vec3(0.0f, 4.5f, 0.0f));
                cellArmModel = glm::rotate(cellArmModel, glm::radians(-90.0f), glm::vec3(1, 0, 0));
                cellArmModel = glm::scale(cellArmModel, glm::vec3(3.5f));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(cellArmModel));
                glBindVertexArray(cellArmsVAO);
                glDrawElements(GL_TRIANGLES, cellArmsIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // Draw punch impact effects (full additive blending for bright energy look)
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for bright energy effects
            
            for (size_t i = 0; i < punchEffects.size(); i++) {
                glm::vec3 effectWorldPos = characterPos + punchEffects[i].position;
                
                if (punchEffects[i].effectType == 0) {
                    // Impact burst - bright flash
                    float scale = punchEffects[i].size * (1.0f + (0.4f - punchEffects[i].life) * 3.0f);
                    
                    glm::mat4 impactModel = glm::mat4(1.0f);
                    impactModel = glm::translate(impactModel, effectWorldPos);
                    impactModel = glm::scale(impactModel, glm::vec3(scale));
                    
                    glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(impactModel));
                    glBindVertexArray(energyBallVAO);  // White/yellow flash
                    glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
                    
                    // Secondary colored burst
                    glm::mat4 colorModel = glm::mat4(1.0f);
                    colorModel = glm::translate(colorModel, effectWorldPos);
                    colorModel = glm::scale(colorModel, glm::vec3(scale * 1.5f));
                    glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(colorModel));
                    if (punchEffects[i].isGokuPunch) {
                        glBindVertexArray(yellowBallVAO);  // Golden for Goku
                    } else {
                        glBindVertexArray(greenBallVAO);   // Green for Cell
                    }
                    glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
                    
                } else if (punchEffects[i].effectType == 1) {
                    // Shockwave ring - expanding circle
                    glm::mat4 waveModel = glm::mat4(1.0f);
                    waveModel = glm::translate(waveModel, effectWorldPos);
                    float ringSize = punchEffects[i].size;
                    waveModel = glm::scale(waveModel, glm::vec3(ringSize, 0.3f, ringSize));
                    
                    glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(waveModel));
                    glBindVertexArray(energyBallVAO);
                    glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
                    
                } else if (punchEffects[i].effectType == 2) {
                    // Speed lines - small streaks radiating outward
                    float angle = (float)i * 0.785f + punchEffects[i].life * 5.0f;
                    float dist = (0.3f - punchEffects[i].life) * 20.0f;
                    glm::vec3 linePos = effectWorldPos + glm::vec3(cos(angle) * dist, sin(angle * 2.0f) * dist * 0.3f, sin(angle) * dist);
                    
                    glm::mat4 lineModel = glm::mat4(1.0f);
                    lineModel = glm::translate(lineModel, linePos);
                    lineModel = glm::rotate(lineModel, angle, glm::vec3(0, 1, 0));
                    float lineScale = punchEffects[i].size * punchEffects[i].life * 3.0f;
                    lineModel = glm::scale(lineModel, glm::vec3(0.2f, 0.2f, lineScale));
                    
                    glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(lineModel));
                    glBindVertexArray(energyBallVAO);
                    glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
                }
            }
            
            // Draw clash energy ball during state 1 (both fighters locked in clash)
            if (dbzBattleState == 1) {
                glm::vec3 clashWorldPos = characterPos + dbzClashPosition;
                float clashEnergy = glm::min(dbzClashTimer * 2.0f, 5.0f);
                float pulse = 1.0f + sin((float)glfwGetTime() * 20.0f) * 0.3f;
                
                // Outer energy sphere
                glm::mat4 clashModel = glm::mat4(1.0f);
                clashModel = glm::translate(clashModel, clashWorldPos);
                clashModel = glm::scale(clashModel, glm::vec3(clashEnergy * pulse));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(clashModel));
                glBindVertexArray(clashBallVAO);
                glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
                
                // Inner bright core
                glm::mat4 coreModel = glm::mat4(1.0f);
                coreModel = glm::translate(coreModel, clashWorldPos);
                coreModel = glm::scale(coreModel, glm::vec3(clashEnergy * 0.5f * pulse));
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(coreModel));
                glBindVertexArray(energyBallVAO);
                glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // Draw explosion during state 2
            if (dbzBattleState == 2 && dbzReturnTimer < 1.5f) {
                glm::vec3 explosionPos = characterPos + dbzClashPosition;
                float explosionSize = 5.0f + dbzReturnTimer * 30.0f;
                float alpha = 1.0f - (dbzReturnTimer / 1.5f);
                
                // Multiple expanding spheres for explosion effect
                for (int j = 0; j < 3; j++) {
                    glm::mat4 explodeModel = glm::mat4(1.0f);
                    explodeModel = glm::translate(explodeModel, explosionPos);
                    float layerSize = explosionSize * (1.0f + j * 0.3f) * alpha;
                    explodeModel = glm::scale(explodeModel, glm::vec3(layerSize));
                    
                    glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(explodeModel));
                    if (j == 0) glBindVertexArray(energyBallVAO);
                    else if (j == 1) glBindVertexArray(yellowBallVAO);
                    else glBindVertexArray(gokuKamehaVAO);
                    glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
                }
            }
            
            // Draw aura particles
            for (size_t i = 0; i < dbzAuraParticles.size(); i++) {
                glm::vec3 auraWorldPos = characterPos + dbzAuraParticles[i].position;
                glm::mat4 auraModel = glm::mat4(1.0f);
                auraModel = glm::translate(auraModel, auraWorldPos);
                auraModel = glm::scale(auraModel, glm::vec3(dbzAuraParticles[i].size * dbzAuraParticles[i].life * 2.0f));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(auraModel));
                if (dbzAuraParticles[i].isGoku) {
                    glBindVertexArray(yellowBallVAO);
                } else {
                    glBindVertexArray(greenBallVAO);
                }
                glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // Draw impact particles
            for (size_t i = 0; i < impactParticles.size(); i++) {
                glm::vec3 particleWorldPos = characterPos + impactParticles[i].position;
                glm::mat4 particleModel = glm::mat4(1.0f);
                particleModel = glm::translate(particleModel, particleWorldPos);
                particleModel = glm::scale(particleModel, glm::vec3(impactParticles[i].size * impactParticles[i].life * 2.0f));
                
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(particleModel));
                if (impactParticles[i].isGoku) {
                    glBindVertexArray(yellowBallVAO);
                } else {
                    glBindVertexArray(greenBallVAO);
                }
                glDrawElements(GL_TRIANGLES, energyBallIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        
        // Only draw portals after 7 seconds in the current world (with smooth fade-in)
        if (timeInWorld > 7.0f) {
            // Calculate fade alpha (0 to 1 over 3 seconds after appearing)
            float portalFadeAlpha = glm::clamp((timeInWorld - 7.0f) / 3.0f, 0.0f, 1.0f);
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            // Draw portal 1 (main portal - front of diamond, -Z direction)
            glUseProgram(portalProgram);
            
            glm::mat4 portalModel = glm::mat4(1.0f);
            portalModel = glm::translate(portalModel, glm::vec3(0.0f, portalY, -15.0f));  // Front of diamond
            portalModel = glm::rotate(portalModel, (float)sin(nowT * 0.5f) * 0.1f, glm::vec3(1, 0, 0));
            portalModel = glm::rotate(portalModel, (float)cos(nowT * 0.3f) * 0.1f, glm::vec3(0, 0, 1));
            
            glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(portalModel));
            glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform1f(glGetUniformLocation(portalProgram, "uTime"), (float)nowT);
            glUniform1i(glGetUniformLocation(portalProgram, "uWorldType"), currentWorld);
            glUniform1f(glGetUniformLocation(portalProgram, "uRadius"), PORTAL_RADIUS);  // 8.0 = main portal
            glUniform1f(glGetUniformLocation(portalProgram, "uFadeAlpha"), portalFadeAlpha);
            
            glBindVertexArray(portalVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, PORTAL_SEGMENTS + 2);
            
            // Draw portal 2 (mirror portal - right of diamond, +X direction)
            glm::mat4 portal2Model = glm::mat4(1.0f);
            portal2Model = glm::translate(portal2Model, glm::vec3(15.0f, portal2Y, 0.0f));  // Right of diamond
            portal2Model = glm::rotate(portal2Model, (float)sin(nowT * 0.7f) * 0.15f, glm::vec3(1, 0, 0));
            portal2Model = glm::rotate(portal2Model, (float)cos(nowT * 0.4f) * 0.15f, glm::vec3(0, 0, 1));
            portal2Model = glm::scale(portal2Model, glm::vec3(0.75f));  // Smaller portal
            
            glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(portal2Model));
            glUniform1f(glGetUniformLocation(portalProgram, "uRadius"), 6.0f);  // 6.0 = mirror portal
            
            glDrawArrays(GL_TRIANGLE_FAN, 0, PORTAL_SEGMENTS + 2);
            
            // Draw portal 3 (speedforce portal - left of diamond, -X direction)
            glm::mat4 portal3Model = glm::mat4(1.0f);
            portal3Model = glm::translate(portal3Model, glm::vec3(-15.0f, portal3Y, 0.0f));  // Left of diamond
            portal3Model = glm::rotate(portal3Model, (float)sin(nowT * 0.9f) * 0.2f, glm::vec3(1, 0, 0));
            portal3Model = glm::rotate(portal3Model, (float)cos(nowT * 0.6f) * 0.2f, glm::vec3(0, 0, 1));
            portal3Model = glm::scale(portal3Model, glm::vec3(0.625f));  // Even smaller
            
            glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(portal3Model));
            glUniform1f(glGetUniformLocation(portalProgram, "uRadius"), 4.0f);  // 4.0 = speedforce portal
            
            glDrawArrays(GL_TRIANGLE_FAN, 0, PORTAL_SEGMENTS + 2);
            
            // Draw portal 4 (hall of mirrors portal - back of diamond, +Z direction)
            glm::mat4 portal4Model = glm::mat4(1.0f);
            portal4Model = glm::translate(portal4Model, glm::vec3(0.0f, portal4Y, 15.0f));  // Back of diamond
            portal4Model = glm::rotate(portal4Model, (float)sin(nowT * 0.6f) * 0.12f, glm::vec3(1, 0, 0));
            portal4Model = glm::rotate(portal4Model, (float)cos(nowT * 0.5f) * 0.12f, glm::vec3(0, 0, 1));
            portal4Model = glm::scale(portal4Model, glm::vec3(0.6f));  // Small elegant portal
            
            glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(portal4Model));
            glUniform1f(glGetUniformLocation(portalProgram, "uRadius"), 5.0f);  // 5.0 = hall of mirrors portal (gold)
            
            glDrawArrays(GL_TRIANGLE_FAN, 0, PORTAL_SEGMENTS + 2);
        }

        // Draw mirrors in Hall of Mirrors world - 4 MIRRORS at corners with real reflections
        // Mirror types: 0=ENLARGED (magnifying), 1=STRETCHED (funhouse), 2&3=NORMAL (flipped)
        if (currentWorld == 4 && !worldExploding) {
            float cornerOffset = MIRROR_DISTANCE * 0.7f;  // Diagonal distance from center
            
            // Enable blending for mirror transparency
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            // Draw the mirror planes with reflection textures
            glUseProgram(mirrorProgram);
            glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform1f(glGetUniformLocation(mirrorProgram, "uTime"), (float)nowT);
            glUniform3fv(glGetUniformLocation(mirrorProgram, "uCameraPos"), 1, glm::value_ptr(cameraPos));
            
            glBindVertexArray(mirrorVAO);
            
            // FRONT-LEFT mirror (corner at -X, -Z) - index 0 - ENLARGED/MAGNIFYING
            if (!mirrorBroken[0]) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mirrorTexture[0]);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uReflectionTex"), 0);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uUseReflectionTex"), 1);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uMirrorType"), 1);  // Enlarged/magnifying
                
                glm::mat4 mirrorModel = glm::mat4(1.0f);
                // Fixed position in world space
                mirrorModel = glm::translate(mirrorModel, glm::vec3(-cornerOffset, 0.0f, -cornerOffset));
                mirrorModel = glm::rotate(mirrorModel, glm::radians(45.0f), glm::vec3(0, 1, 0));  // Face center
                mirrorModel = glm::rotate(mirrorModel, glm::radians(-MIRROR_TILT), glm::vec3(1, 0, 0));  // Tilt backward
                glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(mirrorModel));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            // FRONT-RIGHT mirror (corner at +X, -Z) - index 1 - STRETCHED/FUNHOUSE
            if (!mirrorBroken[1]) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mirrorTexture[1]);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uReflectionTex"), 0);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uUseReflectionTex"), 1);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uMirrorType"), 2);  // Stretched/funhouse
                
                glm::mat4 mirrorModel = glm::mat4(1.0f);
                // Fixed position in world space
                mirrorModel = glm::translate(mirrorModel, glm::vec3(cornerOffset, 0.0f, -cornerOffset));
                mirrorModel = glm::rotate(mirrorModel, glm::radians(-45.0f), glm::vec3(0, 1, 0));  // Face center
                mirrorModel = glm::rotate(mirrorModel, glm::radians(MIRROR_TILT), glm::vec3(1, 0, 0));  // Tilt forward
                glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(mirrorModel));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            // BACK-LEFT mirror (corner at -X, +Z) - index 2 - NORMAL (flipped)
            if (!mirrorBroken[2]) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mirrorTexture[2]);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uReflectionTex"), 0);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uUseReflectionTex"), 1);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uMirrorType"), 0);  // Normal
                
                glm::mat4 mirrorModel = glm::mat4(1.0f);
                // Fixed position in world space
                mirrorModel = glm::translate(mirrorModel, glm::vec3(-cornerOffset, 0.0f, cornerOffset));
                mirrorModel = glm::rotate(mirrorModel, glm::radians(135.0f), glm::vec3(0, 1, 0));  // Face center
                mirrorModel = glm::rotate(mirrorModel, glm::radians(MIRROR_TILT), glm::vec3(1, 0, 0));  // Tilt forward
                glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(mirrorModel));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            // BACK-RIGHT mirror (corner at +X, +Z) - index 3 - NORMAL (flipped horizontally)
            if (!mirrorBroken[3]) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mirrorTexture[3]);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uReflectionTex"), 0);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uUseReflectionTex"), 1);
                glUniform1i(glGetUniformLocation(mirrorProgram, "uMirrorType"), 3);  // Normal but horizontally flipped
                
                glm::mat4 mirrorModel = glm::mat4(1.0f);
                // Fixed position in world space
                mirrorModel = glm::translate(mirrorModel, glm::vec3(cornerOffset, 0.0f, cornerOffset));
                mirrorModel = glm::rotate(mirrorModel, glm::radians(-135.0f), glm::vec3(0, 1, 0));  // Face center
                mirrorModel = glm::rotate(mirrorModel, glm::radians(-MIRROR_TILT), glm::vec3(1, 0, 0));  // Tilt backward
                glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(mirrorModel));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            // Draw shatter particles
            if (!shatterParticles.empty()) {
                glEnable(GL_PROGRAM_POINT_SIZE);
                glUseProgram(shatterProgram);
                glUniformMatrix4fv(glGetUniformLocation(shatterProgram, "uVP"), 1, GL_FALSE, glm::value_ptr(vp));
                
                // Update VBO with particle data
                std::vector<float> particleData;
                for (const auto& p : shatterParticles) {
                    particleData.push_back(p.position.x);
                    particleData.push_back(p.position.y);
                    particleData.push_back(p.position.z);
                    particleData.push_back(p.size.x);
                    particleData.push_back(p.size.y);
                    particleData.push_back(p.alpha);
                }
                
                glBindVertexArray(shatterVAO);
                glBindBuffer(GL_ARRAY_BUFFER, shatterVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, particleData.size() * sizeof(float), particleData.data());
                glDrawArrays(GL_POINTS, 0, (GLsizei)shatterParticles.size());
                glDisable(GL_PROGRAM_POINT_SIZE);
            }
            
            glDisable(GL_BLEND);
        }

        // Draw particles (wind/debris)
        glUseProgram(particleProgram);
        glUniformMatrix4fv(glGetUniformLocation(particleProgram, "uVP"), 1, GL_FALSE, glm::value_ptr(vp));
        glUniform3f(glGetUniformLocation(particleProgram, "uCharPos"), characterPos.x, 0, characterPos.z);
        glBindVertexArray(particleVAO);
        glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteProgram(skyboxProgram);
    
    glDeleteVertexArrays(1, &characterVAO);
    glDeleteBuffers(1, &characterVBO);
    glDeleteBuffers(1, &characterEBO);
    glDeleteProgram(characterProgram);
    
    glDeleteVertexArrays(1, &particleVAO);
    glDeleteBuffers(1, &particleVBO);
    glDeleteProgram(particleProgram);
    
    glDeleteVertexArrays(1, &portalVAO);
    glDeleteBuffers(1, &portalVBO);
    glDeleteProgram(portalProgram);
    
    glDeleteVertexArrays(1, &mirrorVAO);
    glDeleteBuffers(1, &mirrorVBO);
    glDeleteProgram(mirrorProgram);
    
    glDeleteVertexArrays(1, &shatterVAO);
    glDeleteBuffers(1, &shatterVBO);
    glDeleteProgram(shatterProgram);
    
    // Cleanup GLTF bot
    bot.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
