#version 330 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in float aHeight01;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out float vHeight01;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;

    // normal in world (model is basically identity, but keep correct)
    mat3 nrmMat = mat3(transpose(inverse(uModel)));
    vNormal = normalize(nrmMat * aNormal);

    vUV = aUV;
    vHeight01 = aHeight01;

    gl_Position = uProj * uView * world;
}
