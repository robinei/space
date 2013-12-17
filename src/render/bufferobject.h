#ifndef BUFFEROBJECT_H
#define BUFFEROBJECT_H

// requires: opengl.h
#include "util/refcounted.h"

class BufferObject : public RefCounted {
public:
	typedef boost::intrusive_ptr<BufferObject> Ref;

	static Ref create();

    void bind(GLenum target = GL_COPY_READ_BUFFER);
	void unbind();

	void data(GLsizeiptr size, const GLvoid *data = 0, GLenum usage = GL_STATIC_DRAW);
	void write(GLintptr offset, GLsizeiptr size, const GLvoid *data);
	void read(GLintptr offset, GLsizeiptr size, GLvoid *data);

	void copy(BufferObject *dst, GLintptr readoffset, GLintptr writeoffset, GLsizeiptr size);

	void *map(GLenum access = GL_READ_WRITE);
	void *map(GLintptr offset, GLsizeiptr length, GLenum access = GL_READ_WRITE);
	void flush(GLintptr offset, GLsizeiptr length);
	void unmap();

	GLsizeiptr size() { return _size; }

protected:
	BufferObject();
	~BufferObject();

private:
	BufferObject(const BufferObject &);
	BufferObject &operator=(const BufferObject &);

	GLenum _target;
	GLsizeiptr _size;
	GLuint _handle;
};

#endif
