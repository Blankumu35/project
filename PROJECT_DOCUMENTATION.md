# ENDLESS FREEFALL - Project Documentation
#
# Recent Changes & Updates
## Major Changes
- Implemented movement-based character animation and pose logic (arms, tilt, facing direction)
- Added procedural texturing to the bot (metallic blue, panel lines, gold rivets, specular highlights)
- Refactored pose logic for northward movement (reversed arm directions, close fingers for fist)
- Added and later removed the "aura farming" mode (arms crossed, standing pose)
- Improved shader code for stylized rendering
- Modularized codebase for maintainability (split large files into logical modules)
- Ensured compatibility with OpenGL fragment output (RGBA)

## Step-by-Step Development History
1. **Project Initialization**: Set up OpenGL context, window, and basic rendering pipeline. Imported GLTF model and initialized character state.
2. **Movement & Animation**: Implemented movement controls (WASD, space, shift) and character pose logic based on movement direction. Added tilt and facing direction logic.
3. **World Generation**: Developed five unique worlds (Day, Space Verse, Chrome Dimension, Speedforce, Hall of Mirrors) with procedural graphics and transitions.
4. **Mirror Reflections**: Added framebuffer-based real-time reflections for the Hall of Mirrors world, including mirror breaking and shatter particle effects.
5. **Shader Enhancements**: Created custom vertex and fragment shaders for the bot, including procedural metallic texturing, panel lines, rivets, and lighting effects.
6. **Feature Refinement**: Iteratively refined character animation (arm poses, hand/finger logic, pose switching), added and removed special modes (aura farming), and improved input handling.
7. **Codebase Refactoring**: Split large source files into smaller modules (character, world, mirror, camera, input) for better maintainability and readability.
8. **Finalization**: Ensured all features were working, fixed compatibility issues (fragment shader output), and completed documentation.

---

## Overview
**Endless Freefall** is an OpenGL-based 3D graphics project featuring a character in perpetual freefall through multiple procedurally-generated worlds. The project demonstrates various computer graphics techniques including real-time rendering, shader programming, framebuffer-based reflections, particle systems, and procedural content generation.

---

## Table of Contents
1. [Controls](#controls)
2. [World Types](#world-types)
3. [Graphics Features](#graphics-features)
4. [Technical Implementation](#technical-implementation)
5. [Recent Changes & Updates](#recent-changes--updates)

---

## Controls

### Movement
| Key | Action |
|-----|--------|
| **W** | Move forward |
| **S** | Move backward |
| **A** | Move left |
| **D** | Move right |
| **SPACE** | Spread arms (slow down fall, stabilize) |
| **SHIFT** | Dive (speed up fall) |

### Camera
| Input | Action |
|-------|--------|
| **Mouse Movement** | Rotate camera around character |
| **Scroll Wheel** | Zoom in/out |

### World Navigation
| Key | Action |
|-----|--------|
| **1** | Previous world |
| **2** | Next world |
| **ESC** | Exit application |

---

## World Types

The game features **5 unique procedurally-generated worlds**, each with distinct visual aesthetics:

### 1. Day World (World 0)
- Bright blue sky with realistic gradient
- Procedural white fluffy clouds that scroll as you fall
- Golden sun with lens flare and glow effects
- Light scattering effects near the horizon
- Warm, inviting atmosphere

### 2. Space Verse (World 1) - *Previously Night World*
A stunning deep space environment featuring:

#### Stars
- **Multi-layered star fields** with three distinct layers
- **Colored stars**: White/yellow, blue, and orange giant stars
- Stars twinkle and vary in brightness

#### Milky Way Galaxy
- Diagonal band of dense stars stretching across the sky
- Procedural noise-based texture for cosmic dust
- Extra star density within the galactic band

#### Distant Galaxies
- **Spiral Galaxy**: Large spiral with glowing core and arms
- **Elliptical Galaxy**: Smaller, golden-tinted distant galaxy

#### Nebulae
- **Pink Nebula**: Rose-colored gas clouds
- **Blue Nebula**: Cyan-tinted cosmic formations
- Procedurally generated using fractal Brownian motion (FBM)

#### Solar System Planets
| Planet | Features |
|--------|----------|
| **Sun** | Bright star with corona and glow effect |
| **Earth** | Blue marble with continents, oceans, and cloud cover |
| **Mars** | Red planet with terrain variations |
| **Jupiter** | Gas giant with atmospheric bands and Great Red Spot |
| **Saturn** | Complete with detailed ring system |

#### Special Effects
- **Shooting Stars/Meteors**: Occasional streaks across the sky
- Dynamic lighting from the distant sun

### 3. Chrome Dimension (World 2)
- Surreal chrome/mirror-like sky
- Floating mirror panels creating grid patterns
- Metallic reflective aesthetic
- Pinkish-silver horizon
- Abstract, otherworldly atmosphere

### 4. Speedforce (World 3)
- Electric lightning-filled sky inspired by The Flash
- **Dynamic lightning bolts** that flash across the sky
- Red and yellow energy streaks
- Glowing horizon with speed lines
- Flickering ambient lighting
- Intense, high-energy atmosphere

### 5. Hall of Mirrors (World 4)
- Elegant ballroom-style environment
- Warm chandelier lighting with golden particles
- Deep purple/maroon atmosphere
- **Four corner mirrors** with real-time reflections
- Ornate golden-framed mirrors
- Can be "broken" by colliding with them

---

## Graphics Features

### Real-Time Mirror Reflections
The Hall of Mirrors world features **framebuffer-based real-time reflections**:

#### Implementation
- Each of the 4 mirrors has its own **512x512 framebuffer texture**
- Scene is rendered from reflected camera viewpoint for each mirror
- Reflection textures are applied to mirror surfaces
- Proper depth buffering for accurate reflections

#### Mirror Types (4 Different Effects)
| Mirror | Position | Effect | Frame Color |
|--------|----------|--------|-------------|
| **Front-Left** | Corner (-X, -Z) | **Enlarged/Magnifying** - 2.5x zoom | Bright Gold |
| **Front-Right** | Corner (+X, -Z) | **Funhouse/Stretched** - Vertical stretch with animated wave distortion | Purple |
| **Back-Left** | Corner (-X, +Z) | **Normal** - Standard reflection | Gold |
| **Back-Right** | Corner (+X, +Z) | **Horizontally Flipped** - Mirror image | Gold |

#### Mirror Properties
- **Tilt angle**: 15° forward/backward for visual interest
- **Positioned at corners** of the skybox (diagonal from center)
- **Breakable**: Colliding with mirrors shatters them
- **Golden ornate frames** with elegant borders

### Mirror Shattering System
When the character collides with a mirror:
- Mirror **shatters into particles**
- Glass shards fly outward with physics
- Particles have:
  - Random velocities
  - Rotation and tumbling
  - Fade-out over lifetime
  - Reflective glass coloring with rainbow hints
- Breaking all 4 mirrors triggers **world explosion**

### Particle Systems

#### Wind/Debris Particles
- Ambient particles floating in the environment
- Move based on wind patterns
- Respawn around character position
- Add depth and atmosphere to scenes

#### Shatter Particles
- Generated when mirrors break
- Physics-based movement with gravity
- Point sprite rendering with custom shapes
- Alpha fade-out over lifetime

#### Chandelier Particles (Hall of Mirrors)
- Golden sparkles floating in the ballroom
- Add warmth and elegance to the environment

### Portal System
- **Circular portal** that appears periodically during fall
- Swirling animated effect
- Transitions between worlds when passed through
- Flash effect during world transitions
- Color-coded based on destination world

### Character Rendering
- **Humanoid figure** made of colored boxes
- Body parts: Head, torso, arms, legs
- **Tumbling animation** during freefall
- Physics-based rotation that varies over time
- Responds to player input (spread/dive affects tumble)

### Procedural Sky Generation
All skyboxes are **100% procedurally generated** using:
- **Fractal Brownian Motion (FBM)** for clouds and noise
- **Voronoi patterns** for stars
- **Gradient mixing** for sky colors
- **Fresnel effects** for atmospheric scattering
- **Time-based animation** for movement

---

## Technical Implementation

### Shaders Used
| Shader | Purpose |
|--------|---------|
| **Skybox Shader** | Procedural world rendering with 5 world types |
| **Character Shader** | Lit humanoid with reflection support |
| **Mirror Shader** | Reflection texture sampling with distortion effects |
| **Shatter Shader** | Point sprite glass particles |
| **Portal Shader** | Animated swirling portal effect |
| **Particle Shader** | Ambient debris particles |

### OpenGL Features Used
- Vertex Array Objects (VAO) and Vertex Buffer Objects (VBO)
- Element Buffer Objects (EBO) for indexed rendering
- **Framebuffer Objects (FBO)** for mirror reflections
- Renderbuffer Objects for depth attachments
- Multiple texture units
- Blending for transparency
- Depth testing
- Point sprites with `GL_PROGRAM_POINT_SIZE`

### Key Algorithms
1. **FBM (Fractal Brownian Motion)** - Layered noise for organic textures
2. **Voronoi/Worley Noise** - Star field generation
3. **Planar Reflection** - Camera reflection across mirror plane
4. **UV Distortion** - Funhouse mirror effects

---

## Recent Changes & Updates

### Mirror System Overhaul
- ✅ Removed top and bottom mirrors (reduced from 6 to 4)
- ✅ Moved mirrors to **corner positions** of the skybox
- ✅ Added **tilt angles** (15° forward/backward)
- ✅ Implemented **real framebuffer-based reflections**
- ✅ Added **4 different mirror distortion types**:
  - Magnifying (enlarged)
  - Funhouse (stretched with wave animation)
  - Normal
  - Horizontally flipped

### Space Verse (Formerly Night World)
Complete transformation from simple night sky to full space environment:
- ✅ Deep space background
- ✅ Multi-layered colored star fields
- ✅ Milky Way galaxy band
- ✅ Distant spiral and elliptical galaxies
- ✅ Pink and blue nebulae
- ✅ Sun with corona effect
- ✅ Earth with continents and clouds
- ✅ Mars with terrain
- ✅ Jupiter with bands and Great Red Spot
- ✅ Saturn with ring system
- ✅ Shooting stars/meteors

### Code Structure
- Modular world functions in skybox shader
- Separate initialization functions for each system
- Clean separation of render passes
- Efficient uniform caching

---

## File Structure
```
project/
├── final_project.cpp      # Main source file (all-in-one)
├── PROJECT_DOCUMENTATION.md  # This file
├── render/
│   ├── shader.cpp         # Shader utilities
│   └── shader.h
├── shader/
│   ├── bot.vert/.frag     # Bot shaders
│   ├── particle.vert/.frag # Particle shaders
│   └── terrain.vert/.frag  # Terrain shaders
└── model/
    └── bot/               # Bot model assets
```

---

## Build Instructions
```bash
cd build
cmake ..
make final_project
./final_project
```

Or using the VS Code task:
- Run the "build" task (msbuild)

---

## Dependencies
- **GLFW 3.1.2** - Window and input management
- **GLAD 3.3** - OpenGL loader
- **GLM 0.9.7.1** - Mathematics library
- **TinyGLTF 2.9.3** - GLTF model loading (optional)

---

## Future Enhancements (Potential)
- [ ] Add more planets (Neptune, Uranus, Venus, Mercury)
- [ ] Asteroid belt in space verse
- [ ] More mirror distortion types (fish-eye, kaleidoscope)
- [ ] Sound effects for shattering and portals
- [ ] Multiple playable characters
- [ ] Collectibles during freefall
- [ ] Score system based on distance fallen

---

## Credits
- **Course**: Computer Graphics - 4th Year
- **Project Type**: Final Project
- **Engine**: Custom OpenGL 3.3 Core

---

*Last Updated: January 3, 2026*
