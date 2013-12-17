#ifndef OPENGL_H
#define OPENGL_H

#include <exception>
#include <string>

#include <GL/glew.h>
#include "SDL2/SDL_opengl.h"
#include "util/mymath.h"

class OpenGLException : public std::exception {
public:
	OpenGLException(const char *str) : str(str) {
	}
	virtual const char *what() const throw() {
		return str.c_str();
	}
private:
	std::string str;
};

class ShaderException : public OpenGLException {
public:
	ShaderException(const char *str) : OpenGLException(str) {
	}
};

#endif
