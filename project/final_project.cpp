// ============================================================================
// final_project.cpp (FULL)
// - ChunkedTerrain with theme switching (SNOW / GRASS)
// - Center button platform (isolated) to trigger world change (press E)
// - "Speaker behind button" plays a simple beep melody (no extra libs)
// - Terrain texture blending by height + slope (sand/grass/rock/snow)
// - Speedforce skybox is assumed to exist already (optional), but not required
// ============================================================================

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <render/shader.h>
#include "terrain/ChunkedTerrain.h"
#include "character/Bot.h"

#include "particle.h"
#include "stb_image.h"

#include <tiny_gltf.h>

// NOTE: In YOUR CMakeLists, you probably compile only final_project.cpp + shader.cpp + particle.cpp.
// That means stb_image + tinygltf implementations must exist somewhere.
// If you already made tinygltf_impl.cpp / stb_image.cpp, REMOVE these defines here.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cmath>

#ifdef _WIN32
#include <windows.h> // Beep()
#endif

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------
static GLFWwindow* window = nullptr;
static int windowWidth  = 1024;
static int windowHeight = 768;

// ---------------------------------------------------------------------------
// Camera / Player
// ---------------------------------------------------------------------------
static glm::vec3 cameraOffset(0.0f, 170.0f, 520.0f);
static float FoV = 45.0f;
static float zNear = 0.1f;
static float zFar  = 20000.0f;

// Mouse camera control
static float cameraYaw = 0.0f;
static float cameraPitch = -15.0f;
static float lastMouseX = 512.0f;
static float lastMouseY = 384.0f;
static bool firstMouse = true;
static const float mouseSensitivity = 0.15f;

static glm::vec3 playerPos(0.0f, 120.0f, 350.0f);
static float verticalVelocity = 0.0f;
static bool isGrounded = false;

// Input
static bool keyW=false, keyS=false, keyA=false, keyD=false;
static bool keySpace=false, keyShift=false;
static bool keyEPressedEdge=false;  // edge-trigger "press E"

// Lighting & Sky
enum class SkyMode { SUNNY=0, CLOUDY=1 };
static SkyMode currentSky = SkyMode::SUNNY;
static glm::vec3 lightPosition(0.0f, 4000.0f, 0.0f);
static glm::vec3 lightIntensity(2.5e5f, 2.5e5f, 2.5e5f);

// ---------------------------------------------------------------------------
// Center Button / Speaker
// ---------------------------------------------------------------------------
static const glm::vec3 BUTTON_POS(0.0f, 0.0f, 0.0f);
static const float BUTTON_INTERACT_RADIUS = 40.0f;

// ---------------------------------------------------------------------------
// Texture helper
// ---------------------------------------------------------------------------
static GLuint LoadTexture2D(const char* path) {
    int w, h, comp;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path, &w, &h, &comp, 0);
    if (!data) {
        std::cout << "Failed to load texture: " << path << "\n";
        return 0;
    }

    GLenum format = GL_RGB;
    if (comp == 1) format = GL_RED;
    else if (comp == 3) format = GL_RGB;
    else if (comp == 4) format = GL_RGBA;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(data);
    return tex;
}

// ---------------------------------------------------------------------------
// Simple "speaker music" without external libs
// ---------------------------------------------------------------------------
static void PlaySpeakerSoundForTheme(TerrainTheme theme) {
    // Update sky mode based on terrain
    if (theme == TerrainTheme::GRASS) {
        currentSky = SkyMode::SUNNY;
    } else { // SNOW
        currentSky = SkyMode::CLOUDY;
    }
    
#ifdef _WIN32
    // Not real music, but it proves the interaction works and avoids library hell.
    // Replace later with miniaudio/OpenAL if you want actual mp3/wav.
    auto tone = [](int hz, int ms) { Beep(hz, ms); };

    if (theme == TerrainTheme::SNOW) {
        tone(880, 90); tone(988, 90); tone(1175, 120); tone(988, 90); tone(880, 140);
    } else { // GRASS
        tone(659, 120); tone(784, 120); tone(659, 120); tone(523, 180); tone(659, 120);
    }
#else
    (void)theme;
    std::cout << "[Speaker] (no audio on this platform build)\n";
#endif
}

// ---------------------------------------------------------------------------
// Minimal Bot renderer (optional)
// If you still want your animated bot, plug your existing MyBot back in.
// For now we draw a simple capsule-like "marker" cube so this compiles clean.
// ---------------------------------------------------------------------------
static GLuint simpleVAO = 0, simpleVBO = 0;
static GLuint simpleProgram = 0;

static void InitSimplePlayerMesh() {
    // A tiny cube as a placeholder character.
    float v[] = {
        // pos              // normal
        -8,0,-8,  0,1,0,   8,0,-8,  0,1,0,   8,0, 8,  0,1,0,
        -8,0,-8,  0,1,0,   8,0, 8,  0,1,0,  -8,0, 8,  0,1,0,

        -8,40,-8, 0,1,0,   8,40,-8, 0,1,0,  8,40, 8, 0,1,0,
        -8,40,-8, 0,1,0,   8,40, 8, 0,1,0, -8,40, 8, 0,1,0,
    };

    glGenVertexArrays(1, &simpleVAO);
    glGenBuffers(1, &simpleVBO);
    glBindVertexArray(simpleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, simpleVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));

    glBindVertexArray(0);

    // Super small shader for the cube.
    const char* vs =
        "#version 330 core\n"
        "layout(location=0) in vec3 aPos;\n"
        "layout(location=1) in vec3 aN;\n"
        "uniform mat4 uMVP;\n"
        "out vec3 vN;\n"
        "void main(){ vN=aN; gl_Position=uMVP*vec4(aPos,1.0);}";

    const char* fs =
        "#version 330 core\n"
        "in vec3 vN;\n"
        "out vec4 FragColor;\n"
        "void main(){ float l=0.35+0.65*max(vN.y,0.0); FragColor=vec4(vec3(0.2,0.3,0.9)*l,1.0);}";

    // Use your existing shader loader if it supports raw strings? It probably doesn't.
    // So we compile manually here.
    auto compile = [](GLenum type, const char* src)->GLuint{
        GLuint s=glCreateShader(type);
        glShaderSource(s,1,&src,nullptr);
        glCompileShader(s);
        GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
        if(!ok){
            char log[2048]; glGetShaderInfoLog(s,2048,nullptr,log);
            std::cout<<"Simple shader compile error:\n"<<log<<"\n";
        }
        return s;
    };

    GLuint vsh=compile(GL_VERTEX_SHADER,vs);
    GLuint fsh=compile(GL_FRAGMENT_SHADER,fs);
    simpleProgram=glCreateProgram();
    glAttachShader(simpleProgram,vsh);
    glAttachShader(simpleProgram,fsh);
    glLinkProgram(simpleProgram);
    glDeleteShader(vsh);
    glDeleteShader(fsh);
}

// ---------------------------------------------------------------------------
// Input callbacks
// ---------------------------------------------------------------------------
static void key_callback(GLFWwindow* /*w*/, int key, int /*scancode*/, int action, int /*mode*/) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }

    if (key == GLFW_KEY_W) keyW = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_S) keyS = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_A) keyA = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_D) keyD = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_SPACE) keySpace = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_LEFT_SHIFT) keyShift = (action != GLFW_RELEASE);

    // Edge-trigger E
    if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        keyEPressedEdge = true;
    }
}

static void mouse_callback(GLFWwindow* /*window*/, double xpos, double ypos) {
    if (firstMouse) {
        lastMouseX = (float)xpos;
        lastMouseY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastMouseX;
    float yoffset = lastMouseY - (float)ypos; // Reversed: y ranges bottom to top
    lastMouseX = (float)xpos;
    lastMouseY = (float)ypos;

    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    cameraYaw += xoffset;
    cameraPitch += yoffset;

    // Constrain pitch to prevent flipping
    if (cameraPitch > 89.0f) cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;
}

// ---------------------------------------------------------------------------
// Draw a simple "button platform" marker (no textures, just color)
// ---------------------------------------------------------------------------
static GLuint buttonVAO = 0, buttonVBO = 0;
static GLuint buttonProgram = 0;

static void InitButtonMesh() {
    // Flat disk-ish quad (square) + tall "speaker" pillar behind it.
    // We'll just draw two quads as triangles.
    float v[] = {
        // platform (center)
        -40, 0, -40,   40, 0, -40,   40, 0, 40,
        -40, 0, -40,   40, 0, 40,   -40, 0, 40,

        // speaker pillar (behind)
        -10, 0, -90,   10, 0, -90,   10, 70, -90,
        -10, 0, -90,   10, 70, -90, -10, 70, -90,
    };

    glGenVertexArrays(1, &buttonVAO);
    glGenBuffers(1, &buttonVBO);
    glBindVertexArray(buttonVAO);
    glBindBuffer(GL_ARRAY_BUFFER, buttonVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glBindVertexArray(0);

    const char* vs =
        "#version 330 core\n"
        "layout(location=0) in vec3 aPos;\n"
        "uniform mat4 uMVP;\n"
        "void main(){ gl_Position=uMVP*vec4(aPos,1.0); }";
    const char* fs =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec3 uColor;\n"
        "void main(){ FragColor=vec4(uColor,1.0); }";

    auto compile = [](GLenum type, const char* src)->GLuint{
        GLuint s=glCreateShader(type);
        glShaderSource(s,1,&src,nullptr);
        glCompileShader(s);
        GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
        if(!ok){
            char log[2048]; glGetShaderInfoLog(s,2048,nullptr,log);
            std::cout<<"Button shader compile error:\n"<<log<<"\n";
        }
        return s;
    };

    GLuint vsh=compile(GL_VERTEX_SHADER,vs);
    GLuint fsh=compile(GL_FRAGMENT_SHADER,fs);
    buttonProgram=glCreateProgram();
    glAttachShader(buttonProgram,vsh);
    glAttachShader(buttonProgram,fsh);
    glLinkProgram(buttonProgram);
    glDeleteShader(vsh);
    glDeleteShader(fsh);
}

// ---------------------------------------------------------------------------
// Celestial sphere (sun/moon) mesh and rendering
// ---------------------------------------------------------------------------
static GLuint celestialVAO = 0, celestialVBO = 0, celestialEBO = 0;
static GLuint celestialProgram = 0;
static int celestialIndexCount = 0;

static void InitCelestialSphere() {
    // Generate sphere mesh (UV sphere)
    const int latSegments = 16;
    const int lonSegments = 32;
    const float radius = 1.0f; // Will be scaled by model matrix
    
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    // Generate vertices
    for (int lat = 0; lat <= latSegments; ++lat) {
        float theta = lat * glm::pi<float>() / latSegments;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);
        
        for (int lon = 0; lon <= lonSegments; ++lon) {
            float phi = lon * 2.0f * glm::pi<float>() / lonSegments;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);
            
            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;
            
            // Position
            vertices.push_back(x * radius);
            vertices.push_back(y * radius);
            vertices.push_back(z * radius);
        }
    }
    
    // Generate indices
    for (int lat = 0; lat < latSegments; ++lat) {
        for (int lon = 0; lon < lonSegments; ++lon) {
            int first = lat * (lonSegments + 1) + lon;
            int second = first + lonSegments + 1;
            
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
    
    celestialIndexCount = (int)indices.size();
    
    glGenVertexArrays(1, &celestialVAO);
    glGenBuffers(1, &celestialVBO);
    glGenBuffers(1, &celestialEBO);
    
    glBindVertexArray(celestialVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, celestialVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, celestialEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    glBindVertexArray(0);
    
    // Simple emissive shader for sun/moon
    const char* vs =
        "#version 330 core\n"
        "layout(location=0) in vec3 aPos;\n"
        "uniform mat4 uMVP;\n"
        "void main(){ gl_Position=uMVP*vec4(aPos,1.0); }";
    
    const char* fs =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec3 uEmissiveColor;\n"
        "void main(){ FragColor=vec4(uEmissiveColor, 1.0); }";
    
    auto compile = [](GLenum type, const char* src)->GLuint{
        GLuint s=glCreateShader(type);
        glShaderSource(s,1,&src,nullptr);
        glCompileShader(s);
        GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
        if(!ok){
            char log[2048]; glGetShaderInfoLog(s,2048,nullptr,log);
            std::cout<<"Celestial shader compile error:\n"<<log<<"\n";
        }
        return s;
    };
    
    GLuint vsh=compile(GL_VERTEX_SHADER,vs);
    GLuint fsh=compile(GL_FRAGMENT_SHADER,fs);
    celestialProgram=glCreateProgram();
    glAttachShader(celestialProgram,vsh);
    glAttachShader(celestialProgram,fsh);
    glLinkProgram(celestialProgram);
    glDeleteShader(vsh);
    glDeleteShader(fsh);
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

    window = glfwCreateWindow(windowWidth, windowHeight, "Wonderland: Reality Switch Terrain", nullptr, nullptr);
    if (!window) {
        std::cout << "Window creation failed\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (gladLoadGL(glfwGetProcAddress) == 0) {
        std::cout << "GLAD init failed\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    // Sky starts sunny for grass world
    glClearColor(0.52f, 0.72f, 0.92f, 1.0f);

    // Init terrain
    ChunkedTerrain terrain;

    // Higher resolution for better detail
    const float CHUNK_SIZE  = 320.0f;
    const int   VERTS_SIDE  = 129;
    const int   RADIUS      = 6;
    const float UV_TILING   = 8.0f;

    terrain.init(CHUNK_SIZE, VERTS_SIDE, RADIUS, UV_TILING, 220.0f);
    terrain.setTheme(TerrainTheme::GRASS); // start grass world

    // Terrain shader
    GLuint terrainProgram = LoadShadersFromFile("../project/shader/terrain.vert",
                                                "../project/shader/terrain.frag");
    if (!terrainProgram) {
        std::cout << "Failed to compile terrain shaders.\n";
        return -1;
    }

    // Load textures (provide these files in your project)
    GLuint texSand  = LoadTexture2D("../project/model/textures/dirt1.png");
    GLuint texGrass = LoadTexture2D("../project/model/textures/grass_02.png");
    GLuint texRock  = LoadTexture2D("../project/model/textures/rock1.png");
    GLuint texSnow  = LoadTexture2D("../project/model/textures/snow1.png");

    bool haveAllTextures = (texSand && texGrass && texRock && texSnow);
    if (!haveAllTextures) {
        std::cout << "WARNING: Missing some terrain textures. Shader will fall back to procedural colors.\n";
    }

    // Terrain uniforms
    GLint uModelLoc     = glGetUniformLocation(terrainProgram, "uModel");
    GLint uViewLoc      = glGetUniformLocation(terrainProgram, "uView");
    GLint uProjLoc      = glGetUniformLocation(terrainProgram, "uProj");
    GLint uTimeLoc      = glGetUniformLocation(terrainProgram, "uTime");

    GLint uLightPosLoc  = glGetUniformLocation(terrainProgram, "uLightPos");
    GLint uLightIntLoc  = glGetUniformLocation(terrainProgram, "uLightIntensity");

    GLint uTexSandLoc   = glGetUniformLocation(terrainProgram, "uTexSand");
    GLint uTexGrassLoc  = glGetUniformLocation(terrainProgram, "uTexGrass");
    GLint uTexRockLoc   = glGetUniformLocation(terrainProgram, "uTexRock");
    GLint uTexSnowLoc   = glGetUniformLocation(terrainProgram, "uTexSnow");

    GLint uUseTexLoc    = glGetUniformLocation(terrainProgram, "uUseTextures");

    GLint uAmbientLoc   = glGetUniformLocation(terrainProgram, "uAmbient");
    GLint uGammaLoc     = glGetUniformLocation(terrainProgram, "uGamma");
    GLint uExposureLoc  = glGetUniformLocation(terrainProgram, "uExposure");

    GLint uSnowLineLoc  = glGetUniformLocation(terrainProgram, "uSnowLine");
    GLint uSlopeRockLoc = glGetUniformLocation(terrainProgram, "uSlopeRock");
    GLint uThemeLoc     = glGetUniformLocation(terrainProgram, "uThemeMode");

    // Simple player mesh + button mesh
    InitSimplePlayerMesh();
    InitButtonMesh();
    InitCelestialSphere();
    
    // Initialize Bot character
    Bot bot;
    bot.initialize("../project/model/bot/bot.gltf");
    bot.setPosition(playerPos);

    // Particles (optional nice vibe)
    ParticleSystem spores(450);

    // Spore shader program
    GLuint sporeProgram = LoadShadersFromFile("../project/shader/spore.vert",
                                              "../project/shader/spore.frag");
    if (!sporeProgram) {
        std::cout << "Failed to compile spore shaders.\n";
        return -1;
    }

    double lastT = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double nowT = glfwGetTime();
        float dt = float(nowT - lastT);
        lastT = nowT;

        // Bot movement input
        glm::vec3 moveInput(0.0f);
        if (keyW) moveInput.z -= 1.0f;
        if (keyS) moveInput.z += 1.0f;
        if (keyA) moveInput.x -= 1.0f;
        if (keyD) moveInput.x += 1.0f;
        
        // Normalize diagonal movement
        if (glm::length(moveInput) > 0.01f) {
            moveInput = glm::normalize(moveInput);
        }
        
        // Apply sprint multiplier
        if (keyShift && glm::length(moveInput) > 0.01f) {
            moveInput *= 1.8f;  // Sprint is 1.8x faster
        }
        
        // Get terrain height at bot position
        float terrainHeight = terrain.sampleHeightWorldSmooth(bot.getPosition().x, bot.getPosition().z);
        
        // Update bot with collision
        bot.update(dt, moveInput, keySpace, terrainHeight);
        
        // Update player position to follow bot (for legacy systems)
        playerPos = bot.getPosition();

        // Terrain streaming
        terrain.update(playerPos);

        // Button interaction
        float distToButton = glm::length(glm::vec2(playerPos.x - BUTTON_POS.x, playerPos.z - BUTTON_POS.z));
        if (distToButton < BUTTON_INTERACT_RADIUS && keyEPressedEdge) {
            // Cycle theme between SNOW and GRASS only
            TerrainTheme next = terrain.theme();
            if (next == TerrainTheme::SNOW) {
                next = TerrainTheme::GRASS;
            } else {
                next = TerrainTheme::SNOW;
            }

            terrain.setTheme(next);
            PlaySpeakerSoundForTheme(next);
            
            // Update sky colors based on theme
            if (currentSky == SkyMode::SUNNY) {
                glClearColor(0.52f, 0.72f, 0.92f, 1.0f); // Bright blue sunny sky
            } else { // CLOUDY
                glClearColor(0.65f, 0.70f, 0.75f, 1.0f); // Overcast gray sky
            }

            std::cout << "Theme changed to "
                      << (next==TerrainTheme::SNOW ? "SNOW" : "GRASS")
                      << "\n";
        }
        keyEPressedEdge = false;

        // Update spores
        spores.update(dt, playerPos);

        // Camera calculations based on mouse input
        glm::vec3 cameraFront;
        cameraFront.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
        cameraFront.y = sin(glm::radians(cameraPitch));
        cameraFront.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
        cameraFront = glm::normalize(cameraFront);

        // Camera position behind and above player
        float camDist = 520.0f;
        glm::vec3 cameraPos = playerPos - cameraFront * camDist + glm::vec3(0, 170.0f, 0);
        
        // Prevent camera from going below ground
        float groundHeight = terrain.sampleHeightWorld(cameraPos.x, cameraPos.z);
        if (cameraPos.y < groundHeight + 50.0f) {
            cameraPos.y = groundHeight + 50.0f;
        }

        // Matrices
        glm::mat4 proj = glm::perspective(glm::radians(FoV), (float)windowWidth/(float)windowHeight, zNear, zFar);
        glm::mat4 view = glm::lookAt(cameraPos, playerPos, glm::vec3(0,1,0));
        glm::mat4 vp = proj * view;

        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw terrain
        glUseProgram(terrainProgram);
        glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glUniformMatrix4fv(uViewLoc,  1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProjLoc,  1, GL_FALSE, glm::value_ptr(proj));
        glUniform1f(uTimeLoc, (float)nowT);

        glUniform3fv(uLightPosLoc, 1, glm::value_ptr(lightPosition));
        
        // Adjust light intensity based on sky mode
        glm::vec3 currentLightIntensity;
        float currentAmbient;
        if (currentSky == SkyMode::SUNNY) {
            currentLightIntensity = glm::vec3(3.0e5f, 2.9e5f, 2.6e5f); // Warm sunny light
            currentAmbient = 0.15f;
        } else { // CLOUDY
            currentLightIntensity = glm::vec3(2.0e5f, 2.2e5f, 2.4e5f); // Cool overcast light
            currentAmbient = 0.18f; // More ambient in cloudy weather
        }
        
        glUniform3fv(uLightIntLoc, 1, glm::value_ptr(currentLightIntensity));
        glUniform1f(uAmbientLoc, currentAmbient);
        glUniform1f(uGammaLoc, 2.2f);
        glUniform1f(uExposureLoc, 1.4f);  // Slightly higher exposure for vibrant colors

        // Push theme params into shader (snow line / rock slope)
        const TerrainParams& p = terrain.params();
        if (uSnowLineLoc != -1)  glUniform1f(uSnowLineLoc, p.snowLine);
        if (uSlopeRockLoc != -1) glUniform1f(uSlopeRockLoc, p.slopeRock);
        if (uThemeLoc != -1)     glUniform1i(uThemeLoc, (int)terrain.theme());

        glUniform1i(uUseTexLoc, haveAllTextures ? 1 : 0);

        if (haveAllTextures) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texSand);
            glUniform1i(uTexSandLoc, 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, texGrass);
            glUniform1i(uTexGrassLoc, 1);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, texRock);
            glUniform1i(uTexRockLoc, 2);

            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, texSnow);
            glUniform1i(uTexSnowLoc, 3);
        }

        terrain.draw();

        // Draw celestial sphere (sun/moon) at light position
        glUseProgram(celestialProgram);
        GLint uCelestialMVP = glGetUniformLocation(celestialProgram, "uMVP");
        GLint uEmissiveColor = glGetUniformLocation(celestialProgram, "uEmissiveColor");
        
        // Determine sun/moon color and size based on sky mode
        glm::vec3 celestialColor;
        float celestialScale;
        if (currentSky == SkyMode::SUNNY) {
            celestialColor = glm::vec3(1.0f, 0.95f, 0.8f); // Warm yellow/white sun
            celestialScale = 200.0f; // Sun size
        } else { // CLOUDY
            celestialColor = glm::vec3(0.85f, 0.88f, 0.92f); // Cool white/gray sun through clouds
            celestialScale = 180.0f; // Slightly smaller
        }
        
        glm::mat4 mCelestial = glm::translate(glm::mat4(1.0f), lightPosition);
        mCelestial = glm::scale(mCelestial, glm::vec3(celestialScale));
        glm::mat4 mvpCelestial = vp * mCelestial;
        
        glUniformMatrix4fv(uCelestialMVP, 1, GL_FALSE, glm::value_ptr(mvpCelestial));
        glUniform3fv(uEmissiveColor, 1, glm::value_ptr(celestialColor));
        
        // Disable depth test so sun/moon is always visible in sky
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(celestialVAO);
        glDrawElements(GL_TRIANGLES, celestialIndexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);

        // Draw spores (additive glow)
        // If your ParticleSystem expects you to pass the shader program, do that instead.
        // Here we assume your ParticleSystem uses its own program or you already integrated it.
        // If your ParticleSystem requires program: comment next block and use your existing pipeline.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        spores.draw(vp, sporeProgram);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // Draw button platform + speaker (world-space)
        glUseProgram(buttonProgram);
        GLint uMVP = glGetUniformLocation(buttonProgram, "uMVP");
        GLint uColor = glGetUniformLocation(buttonProgram, "uColor");

        glm::mat4 mButton = glm::translate(glm::mat4(1.0f), glm::vec3(BUTTON_POS.x, terrain.sampleHeightWorld(0,0)+1.5f, BUTTON_POS.z));
        glm::mat4 mvpButton = vp * mButton;
        glUniformMatrix4fv(uMVP, 1, GL_FALSE, glm::value_ptr(mvpButton));

        // Platform color changes based on theme
        glm::vec3 c = (terrain.theme()==TerrainTheme::SNOW)  ? glm::vec3(0.6f,0.9f,1.0f) :
                                                                glm::vec3(0.2f,1.0f,0.4f);
        glUniform3fv(uColor, 1, glm::value_ptr(c));
        glBindVertexArray(buttonVAO);
        glDrawArrays(GL_TRIANGLES, 0, 12); // platform + speaker quads
        glBindVertexArray(0);

        // Draw bot character
        bot.render(view, proj, lightPosition, currentLightIntensity);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteProgram(terrainProgram);
    glDeleteProgram(sporeProgram);
    if (texSand)  glDeleteTextures(1, &texSand);
    if (texGrass) glDeleteTextures(1, &texGrass);
    if (texRock)  glDeleteTextures(1, &texRock);
    if (texSnow)  glDeleteTextures(1, &texSnow);

    if (simpleVAO) glDeleteVertexArrays(1, &simpleVAO);
    if (simpleVBO) glDeleteBuffers(1, &simpleVBO);
    if (simpleProgram) glDeleteProgram(simpleProgram);

    if (buttonVAO) glDeleteVertexArrays(1, &buttonVAO);
    if (buttonVBO) glDeleteBuffers(1, &buttonVBO);
    if (buttonProgram) glDeleteProgram(buttonProgram);

    if (celestialVAO) glDeleteVertexArrays(1, &celestialVAO);
    if (celestialVBO) glDeleteBuffers(1, &celestialVBO);
    if (celestialEBO) glDeleteBuffers(1, &celestialEBO);
    if (celestialProgram) glDeleteProgram(celestialProgram);

    glfwTerminate();
    return 0;
}
