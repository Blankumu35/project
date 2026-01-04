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
    // DEBUG: Ignore skinning, use original mesh position/normal
    vec4 worldPos = modelMatrix * vec4(vertexPosition, 1.0);
    fragPosition = vec3(worldPos);
    fragNormal = normalize(mat3(modelMatrix) * vertexNormal);
    fragUV = vertexUV;
    gl_Position = MVP * worldPos;
}
