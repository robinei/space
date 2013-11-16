#ifndef OPENGL_H
#define OPENGL_H

#include <GL/glew.h>
#include <SDL_opengl.h>
#include <glm/glm.hpp>

using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;

#include <exception>
#include <string>

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





#include <boost/intrusive_ptr.hpp>

class RefCounted {
public:
	friend void intrusive_ptr_add_ref(const RefCounted *p) {
		BOOST_ASSERT(p);
		BOOST_ASSERT(p->m_refcount >= 0);
		++p->m_refcount;
	}
	friend void intrusive_ptr_release(const RefCounted *p) {
		BOOST_ASSERT(p);
		BOOST_ASSERT(p->m_refcount >= 0);
		if (--p->m_refcount == 0)
			delete const_cast<RefCounted *>(p);
	}
protected:
	RefCounted() : m_refcount(0) {}
	virtual ~RefCounted() {}
private:
	mutable int m_refcount;
};




#endif
