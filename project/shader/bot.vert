#version 330 core

// Input
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;
layout(location = 3) in vec4 vertexJoints;   // indices of controlling joints
layout(location = 4) in vec4 vertexWeights;  // skinning weights

// Output data, to be interpolated for each fragment
out vec3 worldPosition;
out vec3 worldNormal;

uniform mat4 MVP;

// Max joint count; 25 is used in the lab assets, 64 is safe
uniform mat4 jointMatrices[64];

void main() {
    // Build skinning matrix from up to 4 joints
    int j0 = int(vertexJoints.x);
    int j1 = int(vertexJoints.y);
    int j2 = int(vertexJoints.z);
    int j3 = int(vertexJoints.w);

    mat4 skinMatrix = 
          vertexWeights.x * jointMatrices[j0]
        + vertexWeights.y * jointMatrices[j1]
        + vertexWeights.z * jointMatrices[j2]
        + vertexWeights.w * jointMatrices[j3];

    // Skin the vertex position & normal
    vec4 skinnedPos    = skinMatrix * vec4(vertexPosition, 1.0);
    vec3 skinnedNormal = mat3(skinMatrix) * vertexNormal;

    gl_Position   = MVP * skinnedPos;
    worldPosition = skinnedPos.xyz;
    worldNormal   = normalize(skinnedNormal);
}
