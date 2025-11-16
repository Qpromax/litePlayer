#version 330 core

layout(location = 0) in vec2 aPos;      // 顶点坐标 [-1,1]
layout(location = 1) in vec2 aTexCoord; // 纹理坐标 [0,1]

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
}