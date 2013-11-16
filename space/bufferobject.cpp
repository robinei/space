#include "opengl.h"
#include "bufferobject.h"

BufferObject::Ref BufferObject::create() {
	return new BufferObject();
}

BufferObject::BufferObject() {
	_target = 0;
	_size = 0;
	glGenBuffers(1, &_handle);
}

BufferObject::~BufferObject() {
	glDeleteBuffers(1, &_handle);
}

void BufferObject::bind(GLenum target) {
	_target = target;
	glBindBuffer(_target, _handle);
}

void BufferObject::unbind() {
	assert(_target);
	_target = 0;
	glBindBuffer(_target, 0);
}

void BufferObject::data(GLsizeiptr size, const GLvoid *data, GLenum usage) {
	assert(_target);
	_size = size;
	glBufferData(_target, size, data, usage);
}

void BufferObject::write(GLintptr offset, GLsizeiptr size, const GLvoid *data) {
	assert(_target);
	glBufferSubData(_target, offset, size, data);
}

void BufferObject::read(GLintptr offset, GLsizeiptr size, GLvoid *data) {
	assert(_target);
	glGetBufferSubData(_target, offset, size, data);
}

void BufferObject::copy(BufferObject *dst, GLintptr readoffset, GLintptr writeoffset, GLsizeiptr size) {
	assert(_target);
	glCopyBufferSubData(_target, dst->_target, readoffset, writeoffset, size);
}

void *BufferObject::map(GLenum access) {
	assert(_target);
	return glMapBuffer(_target, access);
}

void *BufferObject::map(GLintptr offset, GLsizeiptr length, GLenum access) {
	assert(_target);
	return glMapBufferRange(_target, offset, length, access);
}

void BufferObject::flush(GLintptr offset, GLsizeiptr length) {
	assert(_target);
	glFlushMappedBufferRange(_target, offset, length);
}

void BufferObject::unmap() {
	assert(_target);
	glUnmapBuffer(_target);
}

