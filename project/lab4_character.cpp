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

#include <vector>
#include <iostream>
#include <iomanip>
#define _USE_MATH_DEFINES
#include <math.h>

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

static GLFWwindow *window;
static int windowWidth = 1024;
static int windowHeight = 768;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);

// Camera
static glm::vec3 eye_center(0.0f, 100.0f, 800.0f);
static glm::vec3 lookat(0.0f, 0.0f, 0.0f);
static glm::vec3 up(0.0f, 1.0f, 0.0f);
static float FoV = 45.0f;
static float zNear = 100.0f;
static float zFar = 1500.0f; 

// Lighting  
static glm::vec3 lightIntensity(5e6f, 5e6f, 5e6f);
static glm::vec3 lightPosition(-275.0f, 500.0f, 800.0f);

// Animation 
static bool playAnimation = true;
static float playbackSpeed = 2.0f;

// The Particle structure
struct Particle {
    glm::vec3 Position;
    glm::vec3 Velocity;
    glm::vec4 Color;
    float Life;
    float Size;
    float CameraDistance;

    // Operator overloading for sorting (if needed later)
    bool operator<(const Particle& that) const {
        return this->CameraDistance > that.CameraDistance;
    }
};

// The Aura System Class
class AuraSystem {
public:
    std::vector<Particle> particles;
    int maxParticles;
    GLuint VAO, VBO;
    GLuint programID;
    GLuint mvpID, colorID;

    AuraSystem() { maxParticles = 1000; } // Constructor
    void initialize();
    void update(float deltaTime, glm::vec3 charPos, float currentAura);
    void render(glm::mat4 view, glm::mat4 proj);
};

void AuraSystem::initialize() {
    // Simple quad for the particle
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

    // Load shaders from the separate shader files
    // Ensure "particle.vert" and "particle.frag" exist in your shader folder!
    programID = LoadShadersFromFile("../project/shader/particle.vert", "../project/shader/particle.frag");
    mvpID = glGetUniformLocation(programID, "MVP");
    colorID = glGetUniformLocation(programID, "particleColor");
}

void AuraSystem::update(float deltaTime, glm::vec3 charPos, float currentAura) {
    // Spawn particles
    int newParticles = (int)(deltaTime * 1000.0f * currentAura);
    if (currentAura > 0.1f && newParticles == 0) newParticles = 1;

    for (int i = 0; i < newParticles; i++) {
        if (particles.size() < maxParticles) {
            Particle p;
            
            // 1. MAKE IT WIDER (Spread)
            // Increased multiplier from 30.0f to 50.0f so particles spawn further out
            float rX = ((rand() % 100) / 50.0f - 1.0f) * 20.0f; 
            float rZ = ((rand() % 100) / 50.0f - 1.0f) * 20.0f;
            
            p.Position = charPos + glm::vec3(rX, 10.0f, rZ); 
            p.Velocity = glm::vec3(0.0f, 50.0f + (currentAura * 100.0f), 0.0f); 
            // Color shifts from Red (low energy) to Gold (Super Saiyan)
            p.Color = glm::mix(glm::vec4(1.0, 0.2, 0.0, 1.0), glm::vec4(1.0, 0.9, 0.2, 1.0), currentAura);
            p.Life = 1.0f;
            p.Size = 10.0f + (currentAura * 20.0f); 
            particles.push_back(p);
        }
    }

    // Update physics
    for (int i = 0; i < particles.size(); i++) {
        Particle &p = particles[i];
        p.Life -= deltaTime * 2.0f; 
        p.Position += p.Velocity * deltaTime;
        p.Color.a = p.Life;

        if (p.Life <= 0.0f) {
            particles.erase(particles.begin() + i);
            i--;
        }
    }
}

void AuraSystem::render(glm::mat4 view, glm::mat4 proj) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending
    glDepthMask(GL_FALSE); // Don't write to depth buffer

    glUseProgram(programID);
    glBindVertexArray(VAO);

    for (const Particle &p : particles) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, p.Position);
        
        // Billboarding: Cancel rotation
        model[0][0] = view[0][0]; model[0][1] = view[1][0]; model[0][2] = view[2][0];
        model[1][0] = view[0][1]; model[1][1] = view[1][1]; model[1][2] = view[2][1];
        model[2][0] = view[0][2]; model[2][1] = view[1][2]; model[2][2] = view[2][2];
        
        model = glm::scale(model, glm::vec3(p.Size));

        glm::mat4 mvp = proj * view * model;

        glUniformMatrix4fv(mvpID, 1, GL_FALSE, &mvp[0][0]);
        glUniform4fv(colorID, 1, &p.Color[0]);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glDepthMask(GL_TRUE); // Restore depth buffer
    glDisable(GL_BLEND);
}

struct MyBot {
	// Shader variable IDs
	GLuint mvpMatrixID;
	GLuint jointMatricesID;
	GLuint lightPositionID;
	GLuint lightIntensityID;
	GLuint programID;

	tinygltf::Model model;

	// Each VAO corresponds to each mesh primitive in the GLTF model
	struct PrimitiveObject {
		GLuint vao;
		std::map<int, GLuint> vbos;
	};
	std::vector<PrimitiveObject> primitiveObjects;

	// Skinning 
	struct SkinObject {
		// Transforms the geometry into the space of the respective joint
		std::vector<glm::mat4> inverseBindMatrices;  

		// Transforms the geometry following the movement of the joints
		std::vector<glm::mat4> globalJointTransforms;

		// Combined transforms
		std::vector<glm::mat4> jointMatrices;
	};
	std::vector<SkinObject> skinObjects;

	// Animation 
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
		std::vector<SamplerObject> samplers;	// Animation data
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
				glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
				transform *= glm::mat4_cast(q);
			}
			if (node.scale.size() == 3) {
				transform = glm::scale(transform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
			}
		}
		return transform;
	}

	void computeLocalNodeTransform(const tinygltf::Model& model, 
        int nodeIndex, 
        std::vector<glm::mat4> &localTransforms)
{
    const tinygltf::Node &node = model.nodes[nodeIndex];
    localTransforms[nodeIndex] = getNodeTransform(node);

    for (int childIndex : node.children) {
        computeLocalNodeTransform(model, childIndex, localTransforms);
    }
}

void computeGlobalNodeTransform(const tinygltf::Model& model, 
        const std::vector<glm::mat4> &localTransforms,
        int nodeIndex, const glm::mat4& parentTransform, 
        std::vector<glm::mat4> &globalTransforms)
{
    glm::mat4 global = parentTransform * localTransforms[nodeIndex];
    globalTransforms[nodeIndex] = global;

    const tinygltf::Node &node = model.nodes[nodeIndex];
    for (int childIndex : node.children) {
        computeGlobalNodeTransform(model, localTransforms, childIndex, global, globalTransforms);
    }
}

	std::vector<SkinObject> prepareSkinning(const tinygltf::Model &model) {
		std::vector<SkinObject> skinObjects;

		// In our Blender exporter, the default number of joints that may influence a vertex is set to 4, just for convenient implementation in shaders.

		for (size_t i = 0; i < model.skins.size(); i++) {
			SkinObject skinObject;

			const tinygltf::Skin &skin = model.skins[i];

			// Read inverseBindMatrices
			const tinygltf::Accessor &accessor = model.accessors[skin.inverseBindMatrices];
			assert(accessor.type == TINYGLTF_TYPE_MAT4);
			const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
			const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
			const float *ptr = reinterpret_cast<const float *>(
            	buffer.data.data() + accessor.byteOffset + bufferView.byteOffset);
			
			skinObject.inverseBindMatrices.resize(accessor.count);
			for (size_t j = 0; j < accessor.count; j++) {
				float m[16];
				memcpy(m, ptr + j * 16, 16 * sizeof(float));
				skinObject.inverseBindMatrices[j] = glm::make_mat4(m);
			}

			assert(skin.joints.size() == accessor.count);

			skinObject.globalJointTransforms.resize(skin.joints.size());
			skinObject.jointMatrices.resize(skin.joints.size());

			// ----------------------------------------------
			
			
			// Compute global transforms of all nodes in T-pose
std::vector<glm::mat4> localNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
std::vector<glm::mat4> globalNodeTransforms(model.nodes.size(), glm::mat4(1.0f));

// Build local transforms from scene roots
const tinygltf::Scene &scene = model.scenes[model.defaultScene];
for (size_t r = 0; r < scene.nodes.size(); ++r) {
    int rootIndex = scene.nodes[r];
    computeLocalNodeTransform(model, rootIndex, localNodeTransforms);
}

// Compute global transforms
glm::mat4 identity(1.0f);
for (size_t r = 0; r < scene.nodes.size(); ++r) {
    int rootIndex = scene.nodes[r];
    computeGlobalNodeTransform(model, localNodeTransforms, rootIndex, identity, globalNodeTransforms);
}

// Fill per-joint data for this skin
for (size_t j = 0; j < skin.joints.size(); ++j) {
    int nodeIndex = skin.joints[j];

    glm::mat4 globalJoint = globalNodeTransforms[nodeIndex];
    skinObject.globalJointTransforms[j] = globalJoint;

    // glTF skinning: jointMatrix = globalTransform * inverseBindMatrix
    skinObject.jointMatrices[j] = globalJoint * skinObject.inverseBindMatrices[j];
}

			// ----------------------------------------------

			skinObjects.push_back(skinObject);
		}
		return skinObjects;
	}

	int findKeyframeIndex(const std::vector<float>& times, float animationTime) 
	{
		int left = 0;
		int right = times.size() - 1;

		while (left <= right) {
			int mid = (left + right) / 2;

			if (mid + 1 < times.size() && times[mid] <= animationTime && animationTime < times[mid + 1]) {
				return mid;
			}
			else if (times[mid] > animationTime) {
				right = mid - 1;
			}
			else { // animationTime >= times[mid + 1]
				left = mid + 1;
			}
		}

		// Target not found
		return times.size() - 2;
	}

	std::vector<AnimationObject> prepareAnimation(const tinygltf::Model &model) 
	{
		std::vector<AnimationObject> animationObjects;
		for (const auto &anim : model.animations) {
			AnimationObject animationObject;
			
			for (const auto &sampler : anim.samplers) {
				SamplerObject samplerObject;

				const tinygltf::Accessor &inputAccessor = model.accessors[sampler.input];
				const tinygltf::BufferView &inputBufferView = model.bufferViews[inputAccessor.bufferView];
				const tinygltf::Buffer &inputBuffer = model.buffers[inputBufferView.buffer];

				assert(inputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				assert(inputAccessor.type == TINYGLTF_TYPE_SCALAR);

				// Input (time) values
				samplerObject.input.resize(inputAccessor.count);

				const unsigned char *inputPtr = &inputBuffer.data[inputBufferView.byteOffset + inputAccessor.byteOffset];
				const float *inputBuf = reinterpret_cast<const float*>(inputPtr);

				// Read input (time) values
				int stride = inputAccessor.ByteStride(inputBufferView);
				for (size_t i = 0; i < inputAccessor.count; ++i) {
					samplerObject.input[i] = *reinterpret_cast<const float*>(inputPtr + i * stride);
				}
				
				const tinygltf::Accessor &outputAccessor = model.accessors[sampler.output];
				const tinygltf::BufferView &outputBufferView = model.bufferViews[outputAccessor.bufferView];
				const tinygltf::Buffer &outputBuffer = model.buffers[outputBufferView.buffer];

				assert(outputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				const unsigned char *outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
				const float *outputBuf = reinterpret_cast<const float*>(outputPtr);

				int outputStride = outputAccessor.ByteStride(outputBufferView);
				
				// Output values
				samplerObject.output.resize(outputAccessor.count);
				
				for (size_t i = 0; i < outputAccessor.count; ++i) {

					if (outputAccessor.type == TINYGLTF_TYPE_VEC3) {
						memcpy(&samplerObject.output[i], outputPtr + i * 3 * sizeof(float), 3 * sizeof(float));
					} else if (outputAccessor.type == TINYGLTF_TYPE_VEC4) {
						memcpy(&samplerObject.output[i], outputPtr + i * 4 * sizeof(float), 4 * sizeof(float));
					} else {
						std::cout << "Unsupport accessor type ..." << std::endl;
					}

				}

				animationObject.samplers.push_back(samplerObject);			
			}

			animationObjects.push_back(animationObject);
		}
		return animationObjects;
	}

	void updateAnimation(
        const tinygltf::Model &model, 
        const tinygltf::Animation &anim, 
        const AnimationObject &animationObject, 
        float time,
        std::vector<glm::mat4> &nodeTransforms) 
{
    // There are many channels so we have to accumulate the transforms 
    for (const auto &channel : anim.channels) {
        
        int targetNodeIndex = channel.target_node;
        const auto &sampler = anim.samplers[channel.sampler];
        
        // Access output (value) data for the channel
        const tinygltf::Accessor &outputAccessor = model.accessors[sampler.output];
        const tinygltf::BufferView &outputBufferView = model.bufferViews[outputAccessor.bufferView];
        const tinygltf::Buffer &outputBuffer = model.buffers[outputBufferView.buffer];

        // Calculate current animation time (wrap if necessary)
        const std::vector<float> &times = animationObject.samplers[channel.sampler].input;
        float animationTime = fmod(time, times.back());
        
        // ----------------------------------------------------------
        // Find keyframe index
        // ----------------------------------------------------------
        int keyframeIndex = findKeyframeIndex(times, animationTime);
        int nextIndex = keyframeIndex + 1;
        if (nextIndex >= static_cast<int>(times.size())) {
            nextIndex = keyframeIndex;    // clamp at end just in case
        }

        float t0 = times[keyframeIndex];
        float t1 = times[nextIndex];
        float factor = 0.0f;
        if (t1 > t0) {
            factor = (animationTime - t0) / (t1 - t0);
            factor = glm::clamp(factor, 0.0f, 1.0f);
        }

        const unsigned char *outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
        const float *outputBuf = reinterpret_cast<const float*>(outputPtr);

        // -----------------------------------------------------------
        // Interpolate for smooth animation
        // -----------------------------------------------------------
        if (channel.target_path == "translation") {
            glm::vec3 translation0, translation1;
            memcpy(&translation0, outputPtr + keyframeIndex * 3 * sizeof(float), 3 * sizeof(float));
            memcpy(&translation1, outputPtr + nextIndex     * 3 * sizeof(float), 3 * sizeof(float));

            glm::vec3 translation = glm::mix(translation0, translation1, factor);
            nodeTransforms[targetNodeIndex] = glm::translate(nodeTransforms[targetNodeIndex], translation);

        } else if (channel.target_path == "rotation") {
            glm::quat rotation0, rotation1;
            memcpy(&rotation0, outputPtr + keyframeIndex * 4 * sizeof(float), 4 * sizeof(float));
            memcpy(&rotation1, outputPtr + nextIndex     * 4 * sizeof(float), 4 * sizeof(float));

            glm::quat rotation = glm::normalize(glm::slerp(rotation0, rotation1, factor));
            nodeTransforms[targetNodeIndex] *= glm::mat4_cast(rotation);

        } else if (channel.target_path == "scale") {
            glm::vec3 scale0, scale1;
            memcpy(&scale0, outputPtr + keyframeIndex * 3 * sizeof(float), 3 * sizeof(float));
            memcpy(&scale1, outputPtr + nextIndex     * 3 * sizeof(float), 3 * sizeof(float));

            glm::vec3 scale = glm::mix(scale0, scale1, factor);
            nodeTransforms[targetNodeIndex] = glm::scale(nodeTransforms[targetNodeIndex], scale);
        }
    }
}

	void updateSkinning(const std::vector<glm::mat4> &nodeTransforms) {

    if (model.skins.empty() || skinObjects.empty()) return;

    // One SkinObject per glTF skin; same indexing
    for (size_t s = 0; s < model.skins.size(); ++s) {
        const tinygltf::Skin &skin = model.skins[s];
        SkinObject &skinObject = skinObjects[s];

        for (size_t j = 0; j < skin.joints.size(); ++j) {
            int nodeIndex = skin.joints[j];

            glm::mat4 globalJoint = nodeTransforms[nodeIndex];
            skinObject.globalJointTransforms[j] = globalJoint;

            // jointMatrix = globalTransform * inverseBindMatrix
            skinObject.jointMatrices[j] = globalJoint * skinObject.inverseBindMatrices[j];
        }
    }
}

	void update(float time) {

    if (model.animations.empty() || model.skins.empty()) {
        return;    // no animation data, nothing to do
    }

    const tinygltf::Animation &animation = model.animations[0];
    const AnimationObject &animationObject = animationObjects[0];

    // Local transforms for *all* nodes this frame
    std::vector<glm::mat4> localNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
    for (size_t i = 0; i < localNodeTransforms.size(); ++i) {
        localNodeTransforms[i] = glm::mat4(1.0f);
    }

    // Apply animated TRS into localNodeTransforms
    updateAnimation(model, animation, animationObject, time, localNodeTransforms);

    // Compute global transforms from scene roots
    std::vector<glm::mat4> globalNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
    const tinygltf::Scene &scene = model.scenes[model.defaultScene];
    glm::mat4 identity(1.0f);

    for (size_t r = 0; r < scene.nodes.size(); ++r) {
        int rootIndex = scene.nodes[r];
        computeGlobalNodeTransform(model, localNodeTransforms, rootIndex, identity, globalNodeTransforms);
    }

    // Feed those global transforms into skinning
    updateSkinning(globalNodeTransforms);
}


	bool loadModel(tinygltf::Model &model, const char *filename) {
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
		if (!warn.empty()) {
			std::cout << "WARN: " << warn << std::endl;
		}

		if (!err.empty()) {
			std::cout << "ERR: " << err << std::endl;
		}

		if (!res)
			std::cout << "Failed to load glTF: " << filename << std::endl;
		else
			std::cout << "Loaded glTF: " << filename << std::endl;

		return res;
	}

	void initialize() {
		// Modify your path if needed
		if (!loadModel(model, "../project/model/bot/bot.gltf")) {
			return;
		}

		// Prepare buffers for rendering 
		primitiveObjects = bindModel(model);

		// Prepare joint matrices
		skinObjects = prepareSkinning(model);

		// Prepare animation data 
		animationObjects = prepareAnimation(model);

		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromFile("../project/shader/bot.vert", "../project/shader/bot.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}

		// Get a handle for GLSL variables
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		lightPositionID = glGetUniformLocation(programID, "lightPosition");
		lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
		jointMatricesID = glGetUniformLocation(programID, "jointMatrices");
	}

	void bindMesh(std::vector<PrimitiveObject> &primitiveObjects,
				tinygltf::Model &model, tinygltf::Mesh &mesh) {

		std::map<int, GLuint> vbos;
		for (size_t i = 0; i < model.bufferViews.size(); ++i) {
			const tinygltf::BufferView &bufferView = model.bufferViews[i];

			int target = bufferView.target;
			
			if (bufferView.target == 0) { 
				// The bufferView with target == 0 in our model refers to 
				// the skinning weights, for 25 joints, each 4x4 matrix (16 floats), totaling to 400 floats or 1600 bytes. 
				// So it is considered safe to skip the warning.
				//std::cout << "WARN: bufferView.target is zero" << std::endl;
				continue;
			}

			const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
			GLuint vbo;
			glGenBuffers(1, &vbo);
			glBindBuffer(target, vbo);
			glBufferData(target, bufferView.byteLength,
						&buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
			
			vbos[i] = vbo;
		}

		// Each mesh can contain several primitives (or parts), each we need to 
		// bind to an OpenGL vertex array object
		for (size_t i = 0; i < mesh.primitives.size(); ++i) {

			tinygltf::Primitive primitive = mesh.primitives[i];
			tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

			GLuint vao;
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);

			for (auto &attrib : primitive.attributes) {
				tinygltf::Accessor accessor = model.accessors[attrib.second];
				int byteStride =
					accessor.ByteStride(model.bufferViews[accessor.bufferView]);
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
				} else {
					std::cout << "vaa missing: " << attrib.first << std::endl;
				}
			}

			// Record VAO for later use
			PrimitiveObject primitiveObject;
			primitiveObject.vao = vao;
			primitiveObject.vbos = vbos;
			primitiveObjects.push_back(primitiveObject);

			glBindVertexArray(0);
		}
	}

	void bindModelNodes(std::vector<PrimitiveObject> &primitiveObjects, 
						tinygltf::Model &model,
						tinygltf::Node &node) {
		// Bind buffers for the current mesh at the node
		if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
			bindMesh(primitiveObjects, model, model.meshes[node.mesh]);
		}

		// Recursive into children nodes
		for (size_t i = 0; i < node.children.size(); i++) {
			assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
			bindModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
		}
	}

	std::vector<PrimitiveObject> bindModel(tinygltf::Model &model) {
		std::vector<PrimitiveObject> primitiveObjects;

		const tinygltf::Scene &scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes.size()));
			bindModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
		}

		return primitiveObjects;
	}

	void drawMesh(const std::vector<PrimitiveObject> &primitiveObjects,
				tinygltf::Model &model, tinygltf::Mesh &mesh) {
		
		for (size_t i = 0; i < mesh.primitives.size(); ++i) 
		{
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

	void drawModelNodes(const std::vector<PrimitiveObject>& primitiveObjects,
						tinygltf::Model &model, tinygltf::Node &node) {
		// Draw the mesh at the node, and recursively do so for children nodes
		if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
			drawMesh(primitiveObjects, model, model.meshes[node.mesh]);
		}
		for (size_t i = 0; i < node.children.size(); i++) {
			drawModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
		}
	}
	void drawModel(const std::vector<PrimitiveObject>& primitiveObjects,
				tinygltf::Model &model) {
		// Draw all nodes
		const tinygltf::Scene &scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			drawModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
		}
	}

	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);
		
		// Set camera
		glm::mat4 mvp = cameraMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// -----------------------------------------------------------------
// Send joint matrices for linear blend skinning
// -----------------------------------------------------------------
if (!skinObjects.empty() && jointMatricesID != -1) {
	const SkinObject &skinObject = skinObjects[0];
	if (!skinObject.jointMatrices.empty()) {
		glUniformMatrix4fv(
			jointMatricesID,
			static_cast<GLsizei>(skinObject.jointMatrices.size()),
			GL_FALSE,
			glm::value_ptr(skinObject.jointMatrices[0])
		);
	}
}

		

		// -----------------------------------------------------------------

		// Set light data 
		glUniform3fv(lightPositionID, 1, &lightPosition[0]);
		glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);

		// Draw the GLTF model
		drawModel(primitiveObjects, model);
	}

	void cleanup() {
		glDeleteProgram(programID);
	}
}; 

int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW." << std::endl;
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // For MacOS
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(windowWidth, windowHeight, "Lab 4", NULL, NULL);
	if (window == NULL)
	{
		std::cerr << "Failed to open a GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetKeyCallback(window, key_callback);

	// Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0)
	{
		std::cerr << "Failed to initialize OpenGL context." << std::endl;
		return -1;
	}

	// Background
	glClearColor(0.2f, 0.2f, 0.25f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// Our 3D character
	MyBot bot;
	bot.initialize();

	// Aura system
	AuraSystem aura;
	aura.initialize();

	// Camera setup
    glm::mat4 viewMatrix, projectionMatrix;
	projectionMatrix = glm::perspective(glm::radians(FoV), (float)windowWidth / windowHeight, zNear, zFar);

	// Time and frame rate tracking
	static double lastTime = glfwGetTime();
	float time = 0.0f;			// Animation time 
	float fTime = 0.0f;			// Time for measuring fps
	unsigned long frames = 0;

	// Main loop
	do
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Update states for animation
        double currentTime = glfwGetTime();
        float deltaTime = float(currentTime - lastTime);
		lastTime = currentTime;

		if (playAnimation) {
			time += deltaTime * playbackSpeed;
			bot.update(time);
		}

		// Update aura
		aura.update(deltaTime, glm::vec3(0,0,0), 1.0f);

		// Rendering
		viewMatrix = glm::lookAt(eye_center, lookat, up);
		glm::mat4 vp = projectionMatrix * viewMatrix;
		bot.render(vp);

		// Render aura
		aura.render(viewMatrix, projectionMatrix);

		// FPS tracking 
		// Count number of frames over a few seconds and take average
		frames++;
		fTime += deltaTime;
		if (fTime > 2.0f) {		
			float fps = frames / fTime;
			frames = 0;
			fTime = 0;
			
			std::stringstream stream;
			stream << std::fixed << std::setprecision(2) << "Lab 4 | Frames per second (FPS): " << fps;
			glfwSetWindowTitle(window, stream.str().c_str());
		}

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (!glfwWindowShouldClose(window));

	// Clean up
	bot.cleanup();

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_UP && action == GLFW_PRESS)
	{
		playbackSpeed += 1.0f;
		if (playbackSpeed > 10.0f) 
			playbackSpeed = 10.0f;
	}

	if (key == GLFW_KEY_DOWN && action == GLFW_PRESS)
	{
		playbackSpeed -= 1.0f;
		if (playbackSpeed < 1.0f) {
			playbackSpeed = 1.0f;
		}
	}

	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
		playAnimation = !playAnimation;
	}

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}
