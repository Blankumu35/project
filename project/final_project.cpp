// ============================================================================
// ENDLESS FREEFALL - Character falling through infinite sky
// ============================================================================

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <render/shader.h>

#include <iostream>
#include <cmath>
#include <vector>

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

// Wind drift
static float windTime = 0.0f;

// World state (0 = day, 1 = night, 2 = chrome, 3 = speedforce, 4 = hall of mirrors)
static int currentWorld = 0;
static float worldTransition = 0.0f;  // For smooth transition effect
static float portalCooldown = 0.0f;   // Prevent instant re-entry

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

static void InitCharacter() {
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
static void GetMirrorInfo(int mirrorIndex, glm::vec3& outPos, glm::vec3& outNormal, float& outYaw) {
    float cornerOffset = MIRROR_DISTANCE * 0.7f;
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
    
    // Draw reflected character
    glUseProgram(charProg);
    
    // Reflect the character's model matrix
    glm::mat4 charModel = glm::mat4(1.0f);
    charModel = glm::translate(charModel, reflectedCharPos);
    // Mirror the rotation based on the mirror normal
    charModel = glm::rotate(charModel, glm::radians(-characterRotX), glm::vec3(1, 0, 0));
    charModel = glm::rotate(charModel, glm::radians(characterRotZ), glm::vec3(0, 0, 1));
    // Flip along mirror normal axis
    charModel = glm::scale(charModel, glm::vec3(1.5f, 1.5f, 1.5f));
    
    glUniformMatrix4fv(glGetUniformLocation(charProg, "uModel"), 1, GL_FALSE, glm::value_ptr(charModel));
    glUniformMatrix4fv(glGetUniformLocation(charProg, "uView"), 1, GL_FALSE, glm::value_ptr(reflView));
    glUniformMatrix4fv(glGetUniformLocation(charProg, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3f(glGetUniformLocation(charProg, "uLightDir"), 0.3f, 0.9f, 0.2f);
    glUniform1f(glGetUniformLocation(charProg, "uReflection"), 0.0f);
    glUniform3fv(glGetUniformLocation(charProg, "uViewPos"), 1, glm::value_ptr(reflectedCamPos));
    
    glBindVertexArray(charVAO);
    glDrawElements(GL_TRIANGLES, charIdxCount, GL_UNSIGNED_INT, 0);
    
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
    
    // Check if all 4 side mirrors broken -> world explosion!
    if (AllMirrorsBroken() && !worldExploding) {
        TriggerWorldExplosion();
    }
}

// Forward declaration
static void ResetMirrors();

// Update shatter particles
static void UpdateShatter(float dt) {
    // Update world explosion
    if (worldExploding) {
        worldExplosionTime += dt;
        if (worldExplosionTime >= worldExplosionDuration) {
            // Explosion complete - return to day world
            worldExploding = false;
            currentWorld = 0;
            worldTransition = 1.0f;
            ResetMirrors();
        }
    }
    
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
    if (currentWorld != 4 || worldExploding) return;  // Only in Hall of Mirrors, not during explosion
    
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
    
    // Check portal 1 (main - cycles through day/night)
    if (portalCooldown <= 0.0f && portalY > -2.0f && portalY < 2.0f) {
        if (distFromCenter < PORTAL_RADIUS * 0.7f) {
            if (currentWorld == 0) currentWorld = 1;
            else if (currentWorld == 1) currentWorld = 0;
            else if (currentWorld == 2) currentWorld = 0;
            else currentWorld = 0;  // Speedforce -> Day
            
            worldTransition = 1.0f;
            portalCooldown = 3.0f;
            portalY = -60.0f;
            portal2Y = -90.0f;
            portal3Y = -130.0f;
            characterPos.x *= 0.5f;
            characterPos.z *= 0.5f;
        }
    }
    
    // Check portal 2 (mirror portal - offset to side)
    if (portalCooldown <= 0.0f && portal2Y > -2.0f && portal2Y < 2.0f) {
        float dist2 = length(glm::vec2(characterPos.x - 12.0f, characterPos.z));
        if (dist2 < 6.0f * 0.7f) {
            if (currentWorld == 0) currentWorld = 2;
            else if (currentWorld == 1) currentWorld = 2;
            else if (currentWorld == 2) currentWorld = 1;
            else currentWorld = 1;  // Speedforce -> Night
            
            worldTransition = 1.0f;
            portalCooldown = 3.0f;
            portalY = -70.0f;
            portal2Y = -100.0f;
            portal3Y = -140.0f;
            characterPos.x *= 0.5f;
            characterPos.z *= 0.5f;
        }
    }
    
    // Check portal 3 (speedforce portal - offset other side)
    if (portalCooldown <= 0.0f && portal3Y > -2.0f && portal3Y < 2.0f) {
        float dist3 = length(glm::vec2(characterPos.x + 10.0f, characterPos.z - 8.0f));
        if (dist3 < 5.0f * 0.7f) {
            if (currentWorld == 3) currentWorld = 2;  // Speedforce -> Chrome
            else if (currentWorld == 4) currentWorld = 3;  // Hall -> Speedforce
            else currentWorld = 3;  // Any -> Speedforce
            
            worldTransition = 1.0f;
            portalCooldown = 3.0f;
            portalY = -80.0f;
            portal2Y = -110.0f;
            portal3Y = -150.0f;
            portal4Y = -180.0f;
            characterPos.x *= 0.5f;
            characterPos.z *= 0.5f;
        }
    }
    
    // Check portal 4 (hall of mirrors portal - behind character at z=15)
    if (portalCooldown <= 0.0f && portal4Y > -2.0f && portal4Y < 2.0f) {
        float dist4 = length(glm::vec2(characterPos.x - 0.0f, characterPos.z - 15.0f));
        if (dist4 < 5.0f * 0.7f) {
            if (currentWorld == 4) currentWorld = 0;  // Hall -> Day
            else {
                currentWorld = 4;  // Any -> Hall of Mirrors
                ResetMirrors();    // Reset all mirrors when entering
            }
            
            worldTransition = 1.0f;
            portalCooldown = 3.0f;
            portalY = -90.0f;
            portal2Y = -120.0f;
            portal3Y = -160.0f;
            portal4Y = -200.0f;
            characterPos.x *= 0.3f;  // Center character more for mirrors
            characterPos.z *= 0.3f;
        }
    }
    
    // Reset portals if they go too far above
    if (portalY > 100.0f) {
        portalY = -50.0f - (rand() / (float)RAND_MAX) * 30.0f;
    }
    if (portal2Y > 120.0f) {
        portal2Y = -70.0f - (rand() / (float)RAND_MAX) * 40.0f;
    }
    if (portal3Y > 140.0f) {
        portal3Y = -90.0f - (rand() / (float)RAND_MAX) * 50.0f;
    }
    if (portal4Y > 160.0f) {
        portal4Y = -100.0f - (rand() / (float)RAND_MAX) * 50.0f;
    }
    
    // Update cooldown and transition
    if (portalCooldown > 0.0f) portalCooldown -= dt;
    if (worldTransition > 0.0f) worldTransition -= dt * 2.0f;
    if (worldTransition < 0.0f) worldTransition = 0.0f;
    
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
    InitParticles();
    InitPortal();
    InitMirrors();

    std::cout << "=== ENDLESS FREEFALL ===\n";
    std::cout << "Watch the character tumble through the infinite sky!\n";
    std::cout << "Fall through PORTALS to travel between worlds!\n";
    std::cout << "  - LARGE portal (purple/orange): Day <-> Night\n";
    std::cout << "  - MEDIUM portal (silver): Chrome Dimension\n";
    std::cout << "  - SMALL portal (red/orange): SPEEDFORCE!\n";
    std::cout << "  - BACK portal (gold): HALL OF MIRRORS!\n";
    std::cout << "Controls:\n";
    std::cout << "  Mouse - Orbit camera around character\n";
    std::cout << "  Scroll - Zoom in/out\n";
    std::cout << "  WASD - Move character (can miss portals!)\n";
    std::cout << "  Space - Spread arms (slow fall)\n";
    std::cout << "  Shift - Dive (fast fall)\n";
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
        
        // Horizontal movement from input - actually move the character!
        float moveSpeed = 15.0f;
        if (keyW) characterVel.z -= moveSpeed * dt;
        if (keyS) characterVel.z += moveSpeed * dt;
        if (keyA) characterVel.x -= moveSpeed * dt;
        if (keyD) characterVel.x += moveSpeed * dt;
        
        // Wind drift (gentle)
        characterVel.x += sin(windTime * 0.5f) * 2.0f * dt;
        characterVel.z += cos(windTime * 0.3f) * 1.5f * dt;
        
        // Dampen horizontal velocity
        characterVel.x *= 0.95f;
        characterVel.z *= 0.95f;
        
        // Actually update character position!
        characterPos.x += characterVel.x * dt;
        characterPos.z += characterVel.z * dt;
        
        // Update fall distance (for sky scrolling)
        totalFallDistance += -characterVel.y * dt;
        
        // Tumbling rotation
        characterRotX += tumbleSpeedX * dt;
        characterRotZ += tumbleSpeedZ * dt;
        
        // Vary tumble speed slightly
        tumbleSpeedX += (sin(windTime * 1.3f) * 5.0f) * dt;
        tumbleSpeedZ += (cos(windTime * 0.9f) * 3.0f) * dt;
        tumbleSpeedX = glm::clamp(tumbleSpeedX, 20.0f, 120.0f);
        tumbleSpeedZ = glm::clamp(tumbleSpeedZ, 15.0f, 80.0f);
        
        // Update particles
        UpdateParticles(dt);
        
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
        
        // Camera follows character position
        glm::vec3 cameraPos(camX + characterPos.x, camY, camZ + characterPos.z);
        glm::vec3 cameraTarget(characterPos.x, 0, characterPos.z);
        
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

        // Draw character at its actual position
        glUseProgram(characterProgram);
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(characterPos.x, 0.0f, characterPos.z));
        model = glm::rotate(model, glm::radians(characterRotX), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(characterRotZ), glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(1.5f));
        
        glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
        
        // Different lighting for each world
        if (currentWorld == 0) {
            glUniform3f(glGetUniformLocation(characterProgram, "uLightDir"), 0.5f, 0.8f, 0.3f);  // Sunlight
        } else if (currentWorld == 1) {
            // Space verse - distant sun light with slight color shift
            glUniform3f(glGetUniformLocation(characterProgram, "uLightDir"), 0.8f, -0.2f, 0.5f);  // Sun direction in space
        } else if (currentWorld == 2) {
            glUniform3f(glGetUniformLocation(characterProgram, "uLightDir"), 0.0f, 1.0f, 0.0f);  // Ambient mirror light
        } else if (currentWorld == 3) {
            // Speedforce - flickering lightning light
            float flicker = 0.5f + lightningFlash * 0.5f;
            glUniform3f(glGetUniformLocation(characterProgram, "uLightDir"), flicker, 0.3f + flicker * 0.5f, 0.1f);
        } else {
            // Hall of Mirrors - warm golden chandelier light
            glUniform3f(glGetUniformLocation(characterProgram, "uLightDir"), 0.3f, 0.9f, 0.2f);
        }
        glUniform1f(glGetUniformLocation(characterProgram, "uReflection"), 0.0f);  // Not a reflection
        glUniform3fv(glGetUniformLocation(characterProgram, "uViewPos"), 1, glm::value_ptr(cameraPos));
        
        glBindVertexArray(characterVAO);
        glDrawElements(GL_TRIANGLES, characterIndexCount, GL_UNSIGNED_INT, 0);
        
        // Draw portal 1 (main portal - night/day cycle)
        glUseProgram(portalProgram);
        
        glm::mat4 portalModel = glm::mat4(1.0f);
        portalModel = glm::translate(portalModel, glm::vec3(0.0f, portalY, 0.0f));
        portalModel = glm::rotate(portalModel, (float)sin(nowT * 0.5f) * 0.1f, glm::vec3(1, 0, 0));
        portalModel = glm::rotate(portalModel, (float)cos(nowT * 0.3f) * 0.1f, glm::vec3(0, 0, 1));
        
        glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(portalModel));
        glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniform1f(glGetUniformLocation(portalProgram, "uTime"), (float)nowT);
        glUniform1i(glGetUniformLocation(portalProgram, "uWorldType"), currentWorld);
        glUniform1f(glGetUniformLocation(portalProgram, "uRadius"), PORTAL_RADIUS);  // 8.0 = main portal
        
        glBindVertexArray(portalVAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, PORTAL_SEGMENTS + 2);
        
        // Draw portal 2 (mirror portal - offset to the side)
        glm::mat4 portal2Model = glm::mat4(1.0f);
        portal2Model = glm::translate(portal2Model, glm::vec3(12.0f, portal2Y, 0.0f));  // Offset to side
        portal2Model = glm::rotate(portal2Model, (float)sin(nowT * 0.7f) * 0.15f, glm::vec3(1, 0, 0));
        portal2Model = glm::rotate(portal2Model, (float)cos(nowT * 0.4f) * 0.15f, glm::vec3(0, 0, 1));
        portal2Model = glm::scale(portal2Model, glm::vec3(0.75f));  // Smaller portal
        
        glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(portal2Model));
        glUniform1f(glGetUniformLocation(portalProgram, "uRadius"), 6.0f);  // 6.0 = mirror portal
        
        glDrawArrays(GL_TRIANGLE_FAN, 0, PORTAL_SEGMENTS + 2);
        
        // Draw portal 3 (speedforce portal - offset to other side)
        glm::mat4 portal3Model = glm::mat4(1.0f);
        portal3Model = glm::translate(portal3Model, glm::vec3(-10.0f, portal3Y, 8.0f));  // Offset other side
        portal3Model = glm::rotate(portal3Model, (float)sin(nowT * 0.9f) * 0.2f, glm::vec3(1, 0, 0));
        portal3Model = glm::rotate(portal3Model, (float)cos(nowT * 0.6f) * 0.2f, glm::vec3(0, 0, 1));
        portal3Model = glm::scale(portal3Model, glm::vec3(0.625f));  // Even smaller
        
        glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(portal3Model));
        glUniform1f(glGetUniformLocation(portalProgram, "uRadius"), 4.0f);  // 4.0 = speedforce portal
        
        glDrawArrays(GL_TRIANGLE_FAN, 0, PORTAL_SEGMENTS + 2);
        
        // Draw portal 4 (hall of mirrors portal - behind character)
        glm::mat4 portal4Model = glm::mat4(1.0f);
        portal4Model = glm::translate(portal4Model, glm::vec3(0.0f, portal4Y, 15.0f));  // Behind character
        portal4Model = glm::rotate(portal4Model, (float)sin(nowT * 0.6f) * 0.12f, glm::vec3(1, 0, 0));
        portal4Model = glm::rotate(portal4Model, (float)cos(nowT * 0.5f) * 0.12f, glm::vec3(0, 0, 1));
        portal4Model = glm::scale(portal4Model, glm::vec3(0.6f));  // Small elegant portal
        
        glUniformMatrix4fv(glGetUniformLocation(portalProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(portal4Model));
        glUniform1f(glGetUniformLocation(portalProgram, "uRadius"), 5.0f);  // 5.0 = hall of mirrors portal (gold)
        
        glDrawArrays(GL_TRIANGLE_FAN, 0, PORTAL_SEGMENTS + 2);

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

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
