#ifndef MESH_H
#define MESH_H

#include <vector>
#include "refcounted.h"
#include "bufferobject.h"

class VertexFormat : public RefCounted {
public:
	typedef boost::intrusive_ptr<VertexFormat> Ref;

	enum Hint {
		// 0 for unknown
		Position = 1,
		Normal,
		Tangent,
		Bitangent,
		Color,
		UV
	};

	struct Attrib {
		Hint hint;
		GLuint index;
		GLint size;
		GLenum type;
		GLboolean normalized;
		GLuint offset;
		GLsizei stride;
		bool integer;
	};

	static Ref create(bool manual_offset_and_stride = false);

	// for manually specifying offset and stride:
	void add_manual(const Attrib &attrib);

	// only use this with automatic offset and stride:
	void add(Hint hint, GLuint index, GLint size, GLenum type, GLboolean normalized = GL_FALSE, bool integer = false);

	const Attrib &operator[](unsigned int index) const;

	std::vector<Attrib>::size_type attrib_count() const;

	std::vector<Attrib>::const_iterator begin() const;
	std::vector<Attrib>::const_iterator end() const;

	GLsizei stride();

private:
	VertexFormat(bool manual_offset_and_stride);
	VertexFormat(const VertexFormat &);
	VertexFormat &operator=(const VertexFormat &);
	friend class Mesh;

	bool _manual_offset_and_stride;
	GLsizei _stride;
	std::vector<Attrib> _attribs;
};


class Mesh : public RefCounted {
public:
	typedef boost::intrusive_ptr<Mesh> Ref;

	static Ref create(GLenum mode, int num_vertex_buffers);

	GLenum mode();

	// either use this:
	GLsizei num_vertexes();
	void set_num_vertexes(GLsizei num);

	// or an index buffer:
	BufferObject::Ref index_buffer();
	GLsizei num_indexes();
	GLenum index_type();
	void set_index_buffer(BufferObject::Ref buf, GLsizei num_indexes, GLenum type = GL_UNSIGNED_SHORT);

	int num_vertex_buffers();
	BufferObject::Ref vertex_buffer(int i);
	VertexFormat::Ref vertex_format(int i);
	void set_vertex_buffer(int i, BufferObject::Ref buf, VertexFormat::Ref format);

	void bind();
	void unbind();

    void render(int offset = 0, int count = 0);
    void render_indexed(int offset = 0, int count = 0);

protected:
	Mesh(GLenum mode, int num_vertex_buffers);
	~Mesh();

private:
	Mesh(const Mesh &);
	Mesh &operator=(const Mesh &);

	GLenum _mode;
	GLuint _vao;

	BufferObject::Ref _index_buffer;
	GLsizei _num_indexes;
	GLenum _index_type;

	GLsizei _num_vertexes;

	int _num_buffers;
	BufferObject::Ref *_buffers;
	VertexFormat::Ref *_formats;

    bool _dirty;
};

#endif
