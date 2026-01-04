#version 330 core
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;
layout(location = 3) in vec4 vertexJoints;
layout(location = 4) in vec4 vertexWeights;

out vec3 fragPosition;
out vec3 fragNormal;
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

    fragPosition = vec3(modelMatrix * skinnedPos);
    fragNormal = normalize(mat3(modelMatrix) * skinnedNormal);
    fragUV = vertexUV;
    gl_Position = MVP * skinnedPos;
}
