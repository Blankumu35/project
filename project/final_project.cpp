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
static const float MIRROR_DISTANCE = 25.0f;  // Distance from center
static const float MIRROR_WIDTH = 40.0f;
static const float MIRROR_HEIGHT = 60.0f;

// Mirror break state (0=left, 1=right, 2=front, 3=back, 4=top, 5=bottom)
static bool mirrorBroken[6] = {false, false, false, false, false, false};
static float mirrorBreakTime[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

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
        
        vec3 nightWorld(vec3 dir) {
            float y = dir.y;
            float cloudScroll = uFallDistance * 0.001;
            
            // Night sky gradient
            vec3 skyTop = vec3(0.02, 0.02, 0.08);      // Deep dark blue
            vec3 skyMid = vec3(0.05, 0.05, 0.15);      // Dark blue
            vec3 horizon = vec3(0.1, 0.08, 0.2);       // Purple horizon
            vec3 skyBottom = vec3(0.03, 0.03, 0.1);    // Dark below
            
            vec3 skyColor;
            if (y > 0.0) {
                float t = pow(y, 0.5);
                skyColor = mix(mix(horizon, skyMid, smoothstep(0.0, 0.3, y)), skyTop, t);
            } else {
                skyColor = mix(horizon, skyBottom, smoothstep(0.0, -0.5, y));
            }
            
            // Add stars
            float starField = stars(dir);
            skyColor += vec3(1.0, 1.0, 0.95) * starField;
            
            // Moon
            vec3 moonDir = normalize(vec3(-0.4, 0.6, -0.5));
            float moonDot = dot(dir, moonDir);
            
            // Moon disc
            if (moonDot > 0.995) {
                float moonDist = 1.0 - moonDot;
                float moonIntensity = smoothstep(0.005, 0.0, moonDist);
                vec3 moonColor = vec3(0.95, 0.95, 0.85);
                
                // Moon texture/craters
                vec2 moonUV = vec2(dir.x - moonDir.x, dir.y - moonDir.y) * 50.0;
                float craters = fbm(vec3(moonUV * 3.0, 0.0));
                moonColor *= 0.8 + craters * 0.3;
                
                skyColor = mix(skyColor, moonColor, moonIntensity);
            }
            
            // Moon glow
            float moonGlow = max(moonDot, 0.0);
            skyColor += vec3(0.2, 0.25, 0.4) * pow(moonGlow, 8.0) * 0.5;
            skyColor += vec3(0.1, 0.15, 0.3) * pow(moonGlow, 2.0) * 0.3;
            
            // Subtle dark clouds lit by moonlight
            vec3 cloudPos = dir * 3.0 + vec3(uTime * 0.01, cloudScroll, uTime * 0.005);
            float clouds = fbm(cloudPos * 1.5);
            clouds = smoothstep(0.5, 0.8, clouds);
            
            float horizonFactor = 1.0 - abs(y);
            clouds *= horizonFactor * horizonFactor * 0.5;
            
            vec3 cloudColor = vec3(0.15, 0.15, 0.25);  // Dark clouds with moonlight tint
            skyColor = mix(skyColor, cloudColor, clouds * 0.4);
            
            // Aurora effect near horizon
            float aurora = sin(dir.x * 5.0 + uTime * 0.3) * sin(dir.z * 3.0 + uTime * 0.2);
            aurora = smoothstep(0.3, 1.0, aurora) * smoothstep(-0.2, 0.3, y) * smoothstep(0.6, 0.2, y);
            vec3 auroraColor = mix(vec3(0.1, 0.8, 0.3), vec3(0.3, 0.2, 0.8), sin(dir.x * 3.0 + uTime) * 0.5 + 0.5);
            skyColor += auroraColor * aurora * 0.3;
            
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
            vec3 nightColor = nightWorld(dir);
            vec3 mirrorColor = mirrorWorld(dir);
            vec3 speedforceColor = speedforceWorld(dir);
            vec3 hallColor = hallOfMirrorsWorld(dir);
            
            // Select world color based on world type
            vec3 finalColor;
            if (uWorldType == 0) {
                finalColor = dayColor;
            } else if (uWorldType == 1) {
                finalColor = nightColor;
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
    
    // Mirror shader - clear/transparent mirror
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
        
        void main() {
            // Clear mirror - very subtle tint
            vec3 baseColor = vec3(0.95, 0.97, 1.0);  // Almost white/clear
            
            // Thin elegant frame border
            float borderX = smoothstep(0.0, 0.015, vTexCoord.x) * smoothstep(1.0, 0.985, vTexCoord.x);
            float borderY = smoothstep(0.0, 0.015, vTexCoord.y) * smoothstep(1.0, 0.985, vTexCoord.y);
            float border = 1.0 - (borderX * borderY);
            vec3 frameColor = vec3(0.75, 0.6, 0.25);  // Gold frame
            
            // Subtle fresnel edge effect
            vec3 V = normalize(uCameraPos - vWorldPos);
            vec3 N = normalize(vNormal);
            float fresnel = pow(1.0 - max(dot(V, N), 0.0), 4.0);
            
            // Very subtle edge highlight
            vec3 mirrorColor = baseColor + vec3(0.05) * fresnel;
            
            // Combine mirror and frame
            vec3 finalColor = mix(mirrorColor, frameColor, border);
            
            // Very transparent in center, more opaque at frame
            float alpha = mix(0.15, 0.9, border);
            
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

// Check if all 4 side mirrors are broken (triggers explosion)
static bool AllSideMirrorsBroken() {
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
    if (AllSideMirrorsBroken() && !worldExploding) {
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
    
    float hitDist = 3.0f;  // Collision distance
    
    // Left mirror (at -X)
    if (!mirrorBroken[0] && characterPos.x < -MIRROR_DISTANCE + hitDist && 
        abs(characterPos.z) < MIRROR_WIDTH/2 && abs(characterPos.y) < MIRROR_HEIGHT/2) {
        BreakMirror(0, glm::vec3(-MIRROR_DISTANCE, 0, 0), glm::vec3(1, 0, 0));
    }
    
    // Right mirror (at +X)
    if (!mirrorBroken[1] && characterPos.x > MIRROR_DISTANCE - hitDist &&
        abs(characterPos.z) < MIRROR_WIDTH/2 && abs(characterPos.y) < MIRROR_HEIGHT/2) {
        BreakMirror(1, glm::vec3(MIRROR_DISTANCE, 0, 0), glm::vec3(-1, 0, 0));
    }
    
    // Front mirror (at -Z)
    if (!mirrorBroken[2] && characterPos.z < -MIRROR_DISTANCE + hitDist &&
        abs(characterPos.x) < MIRROR_WIDTH/2 && abs(characterPos.y) < MIRROR_HEIGHT/2) {
        BreakMirror(2, glm::vec3(0, 0, -MIRROR_DISTANCE), glm::vec3(0, 0, 1));
    }
    
    // Back mirror (at +Z)
    if (!mirrorBroken[3] && characterPos.z > MIRROR_DISTANCE - hitDist &&
        abs(characterPos.x) < MIRROR_WIDTH/2 && abs(characterPos.y) < MIRROR_HEIGHT/2) {
        BreakMirror(3, glm::vec3(0, 0, MIRROR_DISTANCE), glm::vec3(0, 0, -1));
    }
}

// Reset mirrors when entering hall of mirrors world
static void ResetMirrors() {
    for (int i = 0; i < 6; i++) {
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
            else if (currentWorld == 1) worldName = "NIGHT WORLD";
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
            glUniform3f(glGetUniformLocation(characterProgram, "uLightDir"), -0.4f, 0.6f, -0.5f);  // Moonlight
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

        // Draw mirrors in Hall of Mirrors world - 6 MIRRORS (box formation)
        if (currentWorld == 4 && !worldExploding) {
            // Enable blending for reflection clipping fade
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            // First draw all reflections (behind the mirrors) - only for unbroken mirrors
            glUseProgram(characterProgram);
            glUniform1f(glGetUniformLocation(characterProgram, "uReflection"), 0.3f);
            glUniform1f(glGetUniformLocation(characterProgram, "uMirrorBounds"), MIRROR_WIDTH / 2.0f);
            glUniform3f(glGetUniformLocation(characterProgram, "uCharacterPos"), characterPos.x, 0.0f, characterPos.z);
            glBindVertexArray(characterVAO);
            
            // LEFT MIRROR REFLECTION (mirror at -X) - index 0
            if (!mirrorBroken[0]) {
                glUniform1i(glGetUniformLocation(characterProgram, "uMirrorClipMode"), 1);  // Left mirror clips Z
                glm::mat4 leftReflectModel = glm::mat4(1.0f);
                float leftReflectX = -2.0f * MIRROR_DISTANCE - characterPos.x;
                leftReflectModel = glm::translate(leftReflectModel, glm::vec3(leftReflectX, 0.0f, characterPos.z));
                leftReflectModel = glm::rotate(leftReflectModel, glm::radians(-characterRotX), glm::vec3(1, 0, 0));
                leftReflectModel = glm::rotate(leftReflectModel, glm::radians(-characterRotZ), glm::vec3(0, 0, 1));
                leftReflectModel = glm::scale(leftReflectModel, glm::vec3(-1.5f, 1.5f, 1.5f));
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(leftReflectModel));
                glDrawElements(GL_TRIANGLES, characterIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // RIGHT MIRROR REFLECTION (mirror at +X) - index 1
            if (!mirrorBroken[1]) {
                glUniform1i(glGetUniformLocation(characterProgram, "uMirrorClipMode"), 2);  // Right mirror clips Z
                glm::mat4 rightReflectModel = glm::mat4(1.0f);
                float rightReflectX = 2.0f * MIRROR_DISTANCE - characterPos.x;
                rightReflectModel = glm::translate(rightReflectModel, glm::vec3(rightReflectX, 0.0f, characterPos.z));
                rightReflectModel = glm::rotate(rightReflectModel, glm::radians(-characterRotX), glm::vec3(1, 0, 0));
                rightReflectModel = glm::rotate(rightReflectModel, glm::radians(-characterRotZ), glm::vec3(0, 0, 1));
                rightReflectModel = glm::scale(rightReflectModel, glm::vec3(-1.5f, 1.5f, 1.5f));
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(rightReflectModel));
                glDrawElements(GL_TRIANGLES, characterIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // FRONT MIRROR REFLECTION (mirror at -Z, facing +Z) - index 2
            if (!mirrorBroken[2]) {
                glUniform1i(glGetUniformLocation(characterProgram, "uMirrorClipMode"), 3);  // Front mirror clips X
                glm::mat4 frontReflectModel = glm::mat4(1.0f);
                float frontReflectZ = -2.0f * MIRROR_DISTANCE - characterPos.z;
                frontReflectModel = glm::translate(frontReflectModel, glm::vec3(characterPos.x, 0.0f, frontReflectZ));
                frontReflectModel = glm::rotate(frontReflectModel, glm::radians(-characterRotX), glm::vec3(1, 0, 0));
                frontReflectModel = glm::rotate(frontReflectModel, glm::radians(characterRotZ), glm::vec3(0, 0, 1));
                frontReflectModel = glm::scale(frontReflectModel, glm::vec3(1.5f, 1.5f, -1.5f));
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(frontReflectModel));
                glDrawElements(GL_TRIANGLES, characterIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // BACK MIRROR REFLECTION (mirror at +Z, facing -Z) - index 3
            if (!mirrorBroken[3]) {
                glUniform1i(glGetUniformLocation(characterProgram, "uMirrorClipMode"), 4);  // Back mirror clips X
                glm::mat4 backReflectModel = glm::mat4(1.0f);
                float backReflectZ = 2.0f * MIRROR_DISTANCE - characterPos.z;
                backReflectModel = glm::translate(backReflectModel, glm::vec3(characterPos.x, 0.0f, backReflectZ));
                backReflectModel = glm::rotate(backReflectModel, glm::radians(-characterRotX), glm::vec3(1, 0, 0));
                backReflectModel = glm::rotate(backReflectModel, glm::radians(characterRotZ), glm::vec3(0, 0, 1));
                backReflectModel = glm::scale(backReflectModel, glm::vec3(1.5f, 1.5f, -1.5f));
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(backReflectModel));
                glDrawElements(GL_TRIANGLES, characterIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // TOP MIRROR REFLECTION (mirror at +Y, facing down) - index 4
            if (!mirrorBroken[4]) {
                glUniform1i(glGetUniformLocation(characterProgram, "uMirrorClipMode"), 0);  // No clip for top/bottom
                glm::mat4 topReflectModel = glm::mat4(1.0f);
                float topReflectY = 2.0f * MIRROR_DISTANCE;
                topReflectModel = glm::translate(topReflectModel, glm::vec3(characterPos.x, topReflectY, characterPos.z));
                topReflectModel = glm::rotate(topReflectModel, glm::radians(characterRotX), glm::vec3(1, 0, 0));
                topReflectModel = glm::rotate(topReflectModel, glm::radians(characterRotZ), glm::vec3(0, 0, 1));
                topReflectModel = glm::scale(topReflectModel, glm::vec3(1.5f, -1.5f, 1.5f));
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(topReflectModel));
                glDrawElements(GL_TRIANGLES, characterIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // BOTTOM MIRROR REFLECTION (mirror at -Y, facing up) - index 5
            if (!mirrorBroken[5]) {
                glUniform1i(glGetUniformLocation(characterProgram, "uMirrorClipMode"), 0);  // No clip for top/bottom
                glm::mat4 bottomReflectModel = glm::mat4(1.0f);
                float bottomReflectY = -2.0f * MIRROR_DISTANCE;
                bottomReflectModel = glm::translate(bottomReflectModel, glm::vec3(characterPos.x, bottomReflectY, characterPos.z));
                bottomReflectModel = glm::rotate(bottomReflectModel, glm::radians(characterRotX), glm::vec3(1, 0, 0));
                bottomReflectModel = glm::rotate(bottomReflectModel, glm::radians(characterRotZ), glm::vec3(0, 0, 1));
                bottomReflectModel = glm::scale(bottomReflectModel, glm::vec3(1.5f, -1.5f, 1.5f));
                glUniformMatrix4fv(glGetUniformLocation(characterProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(bottomReflectModel));
                glDrawElements(GL_TRIANGLES, characterIndexCount, GL_UNSIGNED_INT, 0);
            }
            
            // Reset uniforms
            glUniform1f(glGetUniformLocation(characterProgram, "uReflection"), 0.0f);
            glUniform1i(glGetUniformLocation(characterProgram, "uMirrorClipMode"), 0);
            
            // Now draw the clear mirror planes - only unbroken
            glUseProgram(mirrorProgram);
            glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform1f(glGetUniformLocation(mirrorProgram, "uTime"), (float)nowT);
            glUniform3fv(glGetUniformLocation(mirrorProgram, "uCameraPos"), 1, glm::value_ptr(cameraPos));
            
            glBindVertexArray(mirrorVAO);
            
            // LEFT mirror - index 0
            if (!mirrorBroken[0]) {
                glm::mat4 leftMirrorModel = glm::mat4(1.0f);
                leftMirrorModel = glm::translate(leftMirrorModel, glm::vec3(-MIRROR_DISTANCE, 0.0f, 0.0f));
                leftMirrorModel = glm::rotate(leftMirrorModel, glm::radians(90.0f), glm::vec3(0, 1, 0));
                glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(leftMirrorModel));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            // RIGHT mirror - index 1
            if (!mirrorBroken[1]) {
                glm::mat4 rightMirrorModel = glm::mat4(1.0f);
                rightMirrorModel = glm::translate(rightMirrorModel, glm::vec3(MIRROR_DISTANCE, 0.0f, 0.0f));
                rightMirrorModel = glm::rotate(rightMirrorModel, glm::radians(-90.0f), glm::vec3(0, 1, 0));
                glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(rightMirrorModel));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            // FRONT mirror - index 2
            if (!mirrorBroken[2]) {
                glm::mat4 frontMirrorModel = glm::mat4(1.0f);
                frontMirrorModel = glm::translate(frontMirrorModel, glm::vec3(0.0f, 0.0f, -MIRROR_DISTANCE));
                glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(frontMirrorModel));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            // BACK mirror - index 3
            if (!mirrorBroken[3]) {
                glm::mat4 backMirrorModel = glm::mat4(1.0f);
                backMirrorModel = glm::translate(backMirrorModel, glm::vec3(0.0f, 0.0f, MIRROR_DISTANCE));
                backMirrorModel = glm::rotate(backMirrorModel, glm::radians(180.0f), glm::vec3(0, 1, 0));
                glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(backMirrorModel));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            // TOP mirror - index 4
            if (!mirrorBroken[4]) {
                glm::mat4 topMirrorModel = glm::mat4(1.0f);
                topMirrorModel = glm::translate(topMirrorModel, glm::vec3(0.0f, MIRROR_DISTANCE, 0.0f));
                topMirrorModel = glm::rotate(topMirrorModel, glm::radians(90.0f), glm::vec3(1, 0, 0));
                glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(topMirrorModel));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            // BOTTOM mirror - index 5
            if (!mirrorBroken[5]) {
                glm::mat4 bottomMirrorModel = glm::mat4(1.0f);
                bottomMirrorModel = glm::translate(bottomMirrorModel, glm::vec3(0.0f, -MIRROR_DISTANCE, 0.0f));
                bottomMirrorModel = glm::rotate(bottomMirrorModel, glm::radians(-90.0f), glm::vec3(1, 0, 0));
                glUniformMatrix4fv(glGetUniformLocation(mirrorProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(bottomMirrorModel));
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
