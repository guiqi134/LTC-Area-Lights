#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader
{
public:
	GLuint ID;

	Shader() = default;
	Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr,
		const char* tescPath = nullptr, const char* tesePath = nullptr);

	// use/active the shader
	void use()
	{
		glUseProgram(ID);
	}

	void deleteProgram()
	{
		glDeleteProgram(ID);
	}

	// utility uniform functions
	void setBool(const std::string& name, bool value) const
	{
		glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
	}
	void setFloat(const std::string& name, GLfloat value) const
	{
		glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
	}
	void setInt(const std::string& name, GLuint value) const
	{
		glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
	}
	void setMat4(const std::string& name, glm::mat4 value) const
	{
		glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
	}
	void setVec3(const std::string& name, glm::vec3 value) const
	{
		glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
	}
	void setVec3(const std::string& name, GLfloat x, GLfloat y, GLfloat z)
	{
		glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
	}
	void setVec2(const std::string& name, glm::vec2 value) const
	{
		glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
	}
	void setVec2(const std::string& name, GLfloat x, GLfloat y)
	{
		glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
	}

private:
	// utility function for checking shader compilation/linking errors.
	// ------------------------------------------------------------------------------
	void checkCompileErrors(GLuint shader, std::string type)
	{
		GLint success;
		GLchar infoLog[1024];
		if (type != "PROGRAM")
		{
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if (!success)
			{
				glGetShaderInfoLog(shader, 1024, NULL, infoLog);
				std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << std::endl;
			}
		}
		else
		{
			glGetProgramiv(shader, GL_LINK_STATUS, &success);
			if (!success)
			{
				glGetProgramInfoLog(shader, 1024, NULL, infoLog);
				std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << std::endl;
			}
		}
	}
};

Shader::Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath, const char* tescPath, const char* tesePath)
{
	// retrieve source code from filePath
	std::string vertexCode;
	std::string fragmentCode;
	std::string geometryCode;
	std::string tescCode;
	std::string teseCode;
	std::ifstream vShaderFile;
	std::ifstream fShaderFile;
	std::ifstream gShaderFile;
	std::ifstream cShaderFile;
	std::ifstream eShaderFile;
	// ensure ifstream objects can throw execptions
	vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	gShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	cShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	eShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	try
	{
		// open files
		vShaderFile.open(vertexPath);
		fShaderFile.open(fragmentPath);
		std::stringstream vShaderStream, fShaderStream;
		// read file's buffer contents into streams
		vShaderStream << vShaderFile.rdbuf();
		fShaderStream << fShaderFile.rdbuf();
		// close file handlers
		vShaderFile.close();
		fShaderFile.close();
		// convert stream into string
		vertexCode = vShaderStream.str();
		fragmentCode = fShaderStream.str();
		// if geometry shader path exists, load the geometry shader
		if (geometryPath != nullptr)
		{
			gShaderFile.open(geometryPath);
			std::stringstream gShaderStream;
			gShaderStream << gShaderFile.rdbuf();
			gShaderFile.close();
			geometryCode = gShaderStream.str();
		}
		// if tessellation control shader path exists, load the tess shader
		if (tescPath != nullptr)
		{
			cShaderFile.open(tescPath);
			std::stringstream cShaderStream;
			cShaderStream << cShaderFile.rdbuf();
			cShaderFile.close();
			tescCode = cShaderStream.str();
		}
		// if tessellation evalution shader path exists, load the tese shader
		if (tesePath != nullptr)
		{
			eShaderFile.open(tesePath);
			std::stringstream eShaderStream;
			eShaderStream << eShaderFile.rdbuf();
			eShaderFile.close();
			teseCode = eShaderStream.str();
		}
	}
	catch (std::ifstream::failure e)
	{
		std::cout << "Error: shader file not successfully read\n";
	}
	const char* vShaderCode = vertexCode.c_str();
	const char* fShaderCode = fragmentCode.c_str();

	// compile shaders
	GLuint vertex, fragment;

	// vertex Shader
	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vShaderCode, NULL);
	glCompileShader(vertex);
	checkCompileErrors(vertex, "VERTEX");
	// fragment Shader
	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fShaderCode, NULL);
	glCompileShader(fragment);
	checkCompileErrors(fragment, "FRAGMENT");
	// geometry shader
	GLuint geometry;
	if (geometryPath != nullptr)
	{
		const char* gShaderCode = geometryCode.c_str();
		geometry = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(geometry, 1, &gShaderCode, NULL);
		glCompileShader(geometry);
		checkCompileErrors(geometry, "GEOMETRY");
	}
	// tessellation control shader
	GLuint tesc;
	if (tescPath != nullptr)
	{
		const char* cShaderCode = tescCode.c_str();
		tesc = glCreateShader(GL_TESS_CONTROL_SHADER);
		glShaderSource(tesc, 1, &cShaderCode, NULL);
		glCompileShader(tesc);
		checkCompileErrors(tesc, "TESS_CONTROL");
	}
	// tessellation evalution shader
	GLuint tese;
	if (tesePath != nullptr)
	{
		const char* eShaderCode = teseCode.c_str();
		tese = glCreateShader(GL_TESS_EVALUATION_SHADER);
		glShaderSource(tese, 1, &eShaderCode, NULL);
		glCompileShader(tese);
		checkCompileErrors(tese, "TESS_EVALUTION");
	}

	// shader Program
	ID = glCreateProgram();
	glAttachShader(ID, vertex);
	if (tescPath != nullptr)
		glAttachShader(ID, tesc);
	if (tesePath != nullptr)
		glAttachShader(ID, tese);
	if (geometryPath != nullptr)
		glAttachShader(ID, geometry);
	glAttachShader(ID, fragment);
	glLinkProgram(ID);
	checkCompileErrors(ID, "PROGRAM");

	// delete shaders
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	if (geometryPath != nullptr)
		glDeleteShader(geometry);
	if (tescPath != nullptr)
		glDeleteShader(tesc);
	if (tesePath != nullptr)
		glDeleteShader(tese);
}