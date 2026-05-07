#pragma once

#include "glad/glad.h"
extern "C"
{
#include "libavutil/frame.h"
}

#include <print>

class Renderer
{
private:
    int    width         = 0;
    int    height        = 0;
    GLuint textures[3]   = {0, 0, 0};
    GLuint shaderProgram = 0;
    GLuint VAO = 0, VBO = 0;
    GLint  texYLoc_ = -1;
    GLint  texULoc_ = -1;
    GLint  texVLoc_ = -1;
    bool   init_ok_ = false;

public:
    explicit Renderer(int w, int h, const char* vertSrc, const char* fragSrc)
    {
        init_ok_ = init(w, h, vertSrc, fragSrc);
    }

    Renderer(const Renderer&)             = delete;
    Renderer operator=(const Renderer&)   = delete;
    Renderer(Renderer&&)                  = delete;
    Renderer operator=(Renderer&&)        = delete;
    auto     operator<=>(const Renderer&) = delete;

    ~Renderer()
    {
        cleanup();
    }

    [[nodiscard]] bool ok() const
    {
        return init_ok_;
    }

    void shutdown()
    {
        cleanup();
    }

    bool init(int w, int h, const char* vertSrc, const char* fragSrc)
    {
        init_ok_ = false;
        cleanup();

        width  = w;
        height = h;

        // YUV texture
        glGenTextures(3, textures);
        for (int i = 0; i < 3; ++i)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, textures[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            if (i == 0)
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
            }
            else
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
            }
        }

        shaderProgram = compileShader(vertSrc, fragSrc);
        if (shaderProgram == 0)
        {
            return false;
        }

        // clang-format off
			float vertices[] = {// pos      // tex
								-1.0f,1.0f,0.0f,0.0f,
								-1.0f,-1.0f,0.0f,1.0f,
                                1.0f,1.0f,1.0f,0.0f,
								1.0f,-1.0f,1.0f,1.0f};
        // clang-format on

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);

        // fix sampler binding
        glUseProgram(shaderProgram);
        texYLoc_ = glGetUniformLocation(shaderProgram, "texY");
        texULoc_ = glGetUniformLocation(shaderProgram, "texU");
        texVLoc_ = glGetUniformLocation(shaderProgram, "texV");
        if (texYLoc_ >= 0)
        {
            glUniform1i(texYLoc_, 0);
        }
        if (texULoc_ >= 0)
        {
            glUniform1i(texULoc_, 1);
        }
        if (texVLoc_ >= 0)
        {
            glUniform1i(texVLoc_, 2);
        }

        init_ok_ = true;
        return true;
    }

    void renderFrame(AVFrame* frame)
    {
        if (!frame)
        {
            return;
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // FFmpeg 帧可能带有 stride（linesize），按行长度上传更稳妥
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[1]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textures[2]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[2]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

private:
    void cleanup()
    {
        if (textures[0] || textures[1] || textures[2])
        {
            glDeleteTextures(3, textures);
            textures[0] = textures[1] = textures[2] = 0;
        }
        if (shaderProgram != 0)
        {
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
        }
        if (VBO != 0)
        {
            glDeleteBuffers(1, &VBO);
            VBO = 0;
        }
        if (VAO != 0)
        {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }
    }

    static GLuint compileShader(const char* vertSrc, const char* fragSrc)
    {
        auto compile = [](GLenum type, const char* src) -> GLuint
        {
            GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);
            GLint success;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                char info[512];
                glGetShaderInfoLog(shader, 512, nullptr, info);
                std::print(stderr, "Shader compile error: {}", info);
                return 0;
            }
            return shader;
        };

        GLuint vert = compile(GL_VERTEX_SHADER, vertSrc);
        GLuint frag = compile(GL_FRAGMENT_SHADER, fragSrc);
        if (!vert || !frag)
        {
            return 0;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vert);
        glAttachShader(program, frag);
        glLinkProgram(program);

        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success)
        {
            char info[512];
            glGetProgramInfoLog(program, 512, nullptr, info);
            std::print(stderr, "Shader link error: {}", info);
            return 0;
        }

        glDeleteShader(vert);
        glDeleteShader(frag);
        return program;
    }
};