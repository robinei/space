#include "opengl.h"
#include "program.h"
#include <glm/gtc/type_ptr.hpp>

#include <cstring>
#include <cassert>
#include <string>


static Program *bound_program = 0;




static void load_file(const char *path, std::string &data_out) {
	FILE *fp = fopen(path, "rb");
	assert(fp);
	fseek(fp, 0, SEEK_END);
	unsigned int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	data_out.resize(size);
	fread(&data_out[0], 1, size, fp);
	fclose(fp);
}



Shader::Ref Shader::create(GLenum type) {
	return new Shader(type);
}

Shader::Ref Shader::compile(GLenum type, const char *text) {
	Shader::Ref shader = Shader::create(type);
	shader->compile(text);
	return shader;
}

Shader::Ref Shader::load(GLenum type, const char *path) {
	Shader::Ref shader = Shader::create(type);
	shader->load(path);
	return shader;
}

Shader::Shader(GLenum type) : _type(type) {
	_shader = glCreateShader(type);
}

Shader::~Shader() {
	glDeleteShader(_shader);
}

void Shader::compile(const char *text) {
	set_source(text);
	compile();
}

void Shader::load(const char *path) {
	std::string data;
	load_file(path, data);
	compile(data.c_str());
}

void Shader::set_source(const char *text) {
	GLint length = strlen(text);
	glShaderSource(_shader, 1, (const GLchar**)&text, &length);
}

void Shader::compile() {
	glCompileShader(_shader);

	GLint result, length;
	glGetShaderiv(_shader, GL_COMPILE_STATUS, &result);
	if (!result) {
		glGetShaderiv(_shader, GL_INFO_LOG_LENGTH, &length);
		std::string log;
		log.resize(length);
		glGetShaderInfoLog(_shader, length, &result, &log[0]);

		throw ShaderException(log.c_str());
	}
}

GLenum Shader::type() {
	return _type;
}






Program::Ref Program::create() {
	return new Program();
}

Program::Program() {
	_program = glCreateProgram();
	assert(_program);
}

Program::~Program() {
	assert(bound_program != this);
	glDeleteProgram(_program);
}

void Program::attach(Shader::Ref shader) {
	glAttachShader(_program, shader->_shader);
	shaderslot(shader->type()) = shader;
}

Shader::Ref Program::attached(GLenum type) {
	return shaderslot(type);
}

void Program::detach(Shader::Ref shader) {
	Shader::Ref &slot = shaderslot(shader->type());
	assert(slot == shader);
	glDetachShader(_program, shader->_shader);
	slot = 0;
}

void Program::detach_all() {
	for (int i = 0; i < 6; ++i)
		if (_shaders[i])
			detach(_shaders[i]);
}

void Program::detach(GLenum type) {
	detach(shaderslot(type));
}

void Program::link() {
	glLinkProgram(_program);

	GLint result, length;
	glGetProgramiv(_program, GL_LINK_STATUS, &result);
	if (!result) {
		glGetProgramiv(_program, GL_INFO_LOG_LENGTH, &length);
		std::string log;
		log.resize(length);
		glGetProgramInfoLog(_program, length, &result, &log[0]);

		throw ShaderException(log.c_str());
	}
}

void Program::bind() {
	assert(!bound_program);
	bound_program = this;
	glUseProgram(_program);
}

void Program::unbind() {
	assert(bound_program == this);
	bound_program = 0;
	glUseProgram(0);
}

GLint Program::attrib_location(const char *name) {
	return glGetAttribLocation(_program, name);
}

void Program::attrib(const char *name, GLuint index) {
	glBindAttribLocation(_program, index, name);
}

GLint Program::uniform_location(const char *name) {
	return glGetUniformLocation(_program, name);
}

void Program::uniform(GLint location, GLfloat value) {
	assert(bound_program == this);
	assert(location >= 0);
	glUniform1f(location, value);
}

void Program::uniform(GLint location, GLint value) {
	assert(bound_program == this);
	assert(location >= 0);
	glUniform1i(location, value);
}

void Program::uniform(GLint location, const vec2 &value) {
	assert(bound_program == this);
	assert(location >= 0);
	glUniform2fv(location, 1, glm::value_ptr(value));
}

void Program::uniform(GLint location, const vec3 &value) {
	assert(bound_program == this);
	assert(location >= 0);
	glUniform3fv(location, 1, glm::value_ptr(value));
}

void Program::uniform(GLint location, const vec4 &value) {
	assert(bound_program == this);
	assert(location >= 0);
	glUniform4fv(location, 1, glm::value_ptr(value));
}

void Program::uniform(GLint location, const mat2 &value) {
	assert(bound_program == this);
	assert(location >= 0);
	glUniformMatrix2fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void Program::uniform(GLint location, const mat3 &value) {
	assert(bound_program == this);
	assert(location >= 0);
	glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void Program::uniform(GLint location, const mat4 &value) {
	assert(bound_program == this);
	assert(location >= 0);
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}


Shader::Ref &Program::shaderslot(GLenum type) {
	switch (type) {
	case GL_COMPUTE_SHADER: return _shaders[0];
	case GL_VERTEX_SHADER: return _shaders[1];
	case GL_TESS_CONTROL_SHADER: return _shaders[2];
	case GL_TESS_EVALUATION_SHADER: return _shaders[3];
	case GL_GEOMETRY_SHADER: return _shaders[4];
	case GL_FRAGMENT_SHADER: return _shaders[5];
	}
	assert(0);
	return _shaders[0];
}
