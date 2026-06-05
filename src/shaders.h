#ifndef SUNSET_SHADERS_H
#define SUNSET_SHADERS_H
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "glad/glad.h"


class program {

public:
    program() : infoLog{} {
    }

    char infoLog[512];
    GLuint prog_id;
    bool ready;

    static program* make_program(
        const std::string& vertex_shader_src,
        const std::string& fragment_shader_src
    ) {
        program* prog = new program;

        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;
        vShaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        try
        {
            // open files
            vShaderFile.open(vertex_shader_src);
            fShaderFile.open(fragment_shader_src);
            std::stringstream vShaderStream, fShaderStream;
            // read file's buffer contents into streams
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();
            // close file handlers
            vShaderFile.close();
            fShaderFile.close();
            // convert stream into string
            vertexCode   = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        }
        catch(std::ifstream::failure e)
        {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
        }
        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);

        int success;
        glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            prog->ready = false;
            glGetShaderInfoLog(vertex, 512, NULL, prog->infoLog);
            std::cout << "l69";
            return prog;
        };


        GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(frag, 1, &fShaderCode, NULL);
        glCompileShader(frag);

        glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            prog->ready = false;
            glGetShaderInfoLog(frag, 512, NULL, prog->infoLog);
            std::cout << "l83";
            return prog;
        };

        GLuint program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, frag);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if  (!success) {
            prog->ready = false;
            glGetProgramInfoLog(program, 512, NULL, prog->infoLog);
            return prog;
        }
        prog->ready = true;
        prog->prog_id = program;
        return prog;

    }
    char* get_log() {
        return this->infoLog;
    };
    void use() {
        glUseProgram(prog_id);
    }
};

#endif //SUNSET_SHADERS_H
