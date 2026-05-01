#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D texY;
uniform sampler2D texU;
uniform sampler2D texV;

void main()
{
    float y = texture(texY, TexCoord).r;
    float u = texture(texU, TexCoord).r - 0.5;
    float v = texture(texV, TexCoord).r - 0.5;

    // BT.601 full range
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;

    FragColor = vec4(clamp(vec3(r, g, b), 0.0, 1.0), 1.0);
}
