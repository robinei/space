#include "opengl.h"
#include "mesh.h"



VertexFormat::Ref VertexFormat::create(bool manual_offset_and_stride) {
	return new VertexFormat(manual_offset_and_stride);
}

VertexFormat::VertexFormat(bool manual_offset_and_stride) : _stride(0) {
	_manual_offset_and_stride = manual_offset_and_stride;
}

void VertexFormat::add_manual(const Attrib &attrib) {
	assert(_manual_offset_and_stride);
	_attribs.push_back(attrib);
}

void VertexFormat::add(Hint hint, GLuint index, GLint size, GLenum type, GLboolean normalized, bool integer) {
	assert(!_manual_offset_and_stride);
	GLsizei bytes = 0;
	switch (type) {
	case GL_BYTE: bytes = 1; break;
	case GL_UNSIGNED_BYTE: bytes = 1; break;
	case GL_SHORT: bytes = 2; break;
	case GL_UNSIGNED_SHORT: bytes = 2; break;
	case GL_INT: bytes = 4; break;
	case GL_UNSIGNED_INT: bytes = 4; break;
	case GL_HALF_FLOAT: assert(!integer); bytes = 2; break;
	case GL_FLOAT: assert(!integer); bytes = 4; break;
	case GL_DOUBLE: assert(!integer); bytes = 8; break;
	case GL_FIXED: assert(!integer); bytes = 4; break; // ???
	case GL_INT_2_10_10_10_REV: assert(!integer); bytes = 4; break; // ???
	case GL_UNSIGNED_INT_2_10_10_10_REV: assert(!integer); bytes = 4; break; // ???
	case GL_UNSIGNED_INT_10F_11F_11F_REV: assert(!integer); bytes = 4; break; // ???
	}
	bytes *= size;
	Attrib attrib;
	attrib.hint = hint;
	attrib.index = index;
	attrib.size = size;
	attrib.type = type;
	attrib.normalized = normalized;
	attrib.offset = _stride;
	attrib.integer = integer;
	_stride += bytes;
	_attribs.push_back(attrib);
}

GLsizei VertexFormat::stride() {
	return _stride;
}

std::vector<VertexFormat::Attrib>::size_type VertexFormat::attrib_count() const {
	return _attribs.size();
}

const VertexFormat::Attrib &VertexFormat::operator[](unsigned int index) const {
	return _attribs[index];
}

std::vector<VertexFormat::Attrib>::const_iterator VertexFormat::begin() const {
	return _attribs.begin();
}

std::vector<VertexFormat::Attrib>::const_iterator VertexFormat::end() const {
	return _attribs.end();
}






Mesh::Ref Mesh::create(GLenum mode, int num_vertex_buffers) {
	return new Mesh(mode, num_vertex_buffers);
}


Mesh::Mesh(GLenum mode, int num_vertex_buffers) {
	_mode = mode;
	glGenVertexArrays(1, &_vao);

	_index_buffer = 0;
	_num_indexes = 0;
	_index_type = GL_UNSIGNED_SHORT;

	_num_vertexes = 0;

	_num_buffers = num_vertex_buffers;
	_buffers = new BufferObject::Ref[num_vertex_buffers];
	_formats = new VertexFormat::Ref[num_vertex_buffers];
}

Mesh::~Mesh() {
	glDeleteVertexArrays(1, &_vao);
	delete[] _buffers;
	delete[] _formats;
}

GLenum Mesh::mode() {
	return _mode;
}

GLsizei Mesh::num_vertexes() {
	return _num_vertexes;
}

void Mesh::set_num_vertexes(GLsizei num) {
	_num_vertexes = num;
}

BufferObject::Ref Mesh::index_buffer() {
	return _index_buffer;
}

GLsizei Mesh::num_indexes() {
	return _num_indexes;
}

GLenum Mesh::index_type() {
	return _index_type;
}

void Mesh::set_index_buffer(BufferObject::Ref buf, GLsizei num_indexes, GLenum type) {
	_index_buffer = buf;
	_num_indexes = num_indexes;
	_index_type = type;
}

int Mesh::num_vertex_buffers() {
	return _num_buffers;
}

BufferObject::Ref Mesh::vertex_buffer(int i) {
	assert(i < _num_buffers);
	return _buffers[i];
}

VertexFormat::Ref Mesh::vertex_format(int i) {
	assert(i < _num_buffers);
	return _formats[i];
}

void Mesh::set_vertex_buffer(int i, BufferObject::Ref buf, VertexFormat::Ref format) {
	_buffers[i] = buf;
	_formats[i] = format;

	for (auto &attrib : *format) {
		glEnableVertexAttribArray(attrib.index);
		GLsizei stride = format->stride();
		if (format->_manual_offset_and_stride)
			stride = attrib.stride;
		if (!attrib.integer)
			glVertexAttribPointer(attrib.index, attrib.size, attrib.type, attrib.normalized,
			stride, (const GLvoid *)attrib.offset);
		else
			glVertexAttribIPointer(attrib.index, attrib.size, attrib.type,
			stride, (const GLvoid *)attrib.offset);
	}
}

void Mesh::bind() {
	glBindVertexArray(_vao);
}

void Mesh::unbind() {
	glBindVertexArray(0);
}

void Mesh::render() {
	if (_index_buffer)
		glDrawElements(_mode, _num_indexes, _index_type, 0);
	else
		glDrawArrays(_mode, 0, _num_vertexes);
}


