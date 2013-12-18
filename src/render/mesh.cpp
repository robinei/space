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

    _dirty = true;

    _radius = 0;
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
    _dirty = true;
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
    _dirty = true;
}

void Mesh::bind() {
    glBindVertexArray(_vao);

    if (!_dirty)
        return;
    _dirty = false;

    for (int i = 0; i < _num_buffers; ++i) {
        VertexFormat *fmt = _formats[i].get();
        BufferObject *buf = _buffers[i].get();
        if (!fmt)
            continue;
        if (!buf)
            continue;

        buf->bind(GL_ARRAY_BUFFER);

        for (auto &attrib : *fmt) {
            glEnableVertexAttribArray(attrib.index);
            GLsizei stride = fmt->stride();
            if (fmt->_manual_offset_and_stride)
                stride = attrib.stride;
            if (!attrib.integer)
                glVertexAttribPointer(attrib.index, attrib.size, attrib.type, attrib.normalized,
                stride, (const GLvoid *)attrib.offset);
            else
                glVertexAttribIPointer(attrib.index, attrib.size, attrib.type,
                stride, (const GLvoid *)attrib.offset);
        }
    }

    if (_index_buffer)
        _index_buffer->bind(GL_ELEMENT_ARRAY_BUFFER);
}

void Mesh::unbind() {
	glBindVertexArray(0);
}

void Mesh::render(int offset, int count) {
    if (!count)
        count = _num_vertexes;
    if (count <= 0)
        return;
    assert(offset + count <= _num_vertexes);
    glDrawArrays(_mode, offset, count);
}

void Mesh::render_indexed(int offset, int count) {
    assert(_index_buffer);
    assert(_num_indexes > 0);
    if (!count)
        count = _num_indexes;
    assert(offset + count <= _num_indexes);
    int prim_size = 0;
    switch (_index_type) {
    case GL_UNSIGNED_BYTE: prim_size = 1; break;
    case GL_UNSIGNED_SHORT: prim_size = 2; break;
    case GL_UNSIGNED_INT: prim_size = 4; break;
    default: assert(0); break;
    }
    glDrawElements(_mode, count, _index_type, reinterpret_cast<void *>(offset*prim_size));
}






struct MeshFileHeader {
    uint32_t fourcc;
    uint32_t version;

    uint32_t num_vertices;
    uint32_t num_indices;

    uint32_t have_normals;
    uint32_t have_tangents;
    uint32_t have_bitangents;
    uint32_t num_texcoord_sets;
    uint32_t num_color_sets;
};



#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


static Mesh::Ref do_load_mesh(aiMesh *aimesh, bool want_normals) {
    std::vector<GLfloat> verts;
    std::vector<GLuint> indices;

    assert(aimesh->HasPositions());
    if (want_normals) {
        assert(aimesh->HasNormals());
    }

    float radius = 0;

    for (unsigned int i = 0; i < aimesh->mNumVertices; ++i) {
        aiVector3D v = aimesh->mVertices[i];
        verts.push_back(v.x);
        verts.push_back(v.y);
        verts.push_back(v.z);

        if (want_normals) {
            aiVector3D n = aimesh->mNormals[i];
            verts.push_back(n.x);
            verts.push_back(n.y);
            verts.push_back(n.z);
        }

        float len = v.Length();
        if (len > radius)
            radius = len;
    }

    for (unsigned int i = 0; i < aimesh->mNumFaces; ++i) {
        aiFace f = aimesh->mFaces[i];
        if (f.mNumIndices != 3)
            return 0;
        indices.push_back(f.mIndices[0]);
        indices.push_back(f.mIndices[1]);
        indices.push_back(f.mIndices[2]);
    }

    VertexFormat::Ref format = VertexFormat::create();
    format->add(VertexFormat::Position, 0, 3, GL_FLOAT);
    if (want_normals)
        format->add(VertexFormat::Normal, 1, 3, GL_FLOAT);

    BufferObject::Ref index_buffer = BufferObject::create();
    index_buffer->bind();
    index_buffer->data(sizeof(indices[0])*indices.size(), &indices[0]);
    index_buffer->unbind();

    BufferObject::Ref vertex_buffer = BufferObject::create();
    vertex_buffer->bind();
    vertex_buffer->data(sizeof(verts[0])*verts.size(), &verts[0]);
    vertex_buffer->unbind();

    Mesh::Ref mesh = Mesh::create(GL_TRIANGLES, 1);
    mesh->set_vertex_buffer(0, vertex_buffer, format);
    mesh->set_index_buffer(index_buffer, indices.size(), GL_UNSIGNED_INT);

    mesh->set_radius(radius);

    return mesh;
}

static Mesh::Ref load_mesh(const char *path, bool want_normals = true) {
    Assimp::Importer importer;

    unsigned int flags = aiProcess_Triangulate |
        aiProcess_SortByPType |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes |
        aiProcess_OptimizeGraph |
        aiProcess_PreTransformVertices;
    if (want_normals)
        flags |= aiProcess_GenSmoothNormals;
    const aiScene *scene = importer.ReadFile(path, flags);

    if (!scene) {
        printf("import error: %s\n", importer.GetErrorString());
        return 0;
    }

    printf("num meshes: %d\n\n", scene->mNumMeshes);

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh *aimesh = scene->mMeshes[i];
        printf("  %s -\tverts: %d,\tfaces: %d,\tmat: %d,\thas colors: %d\n", aimesh->mName.C_Str(), aimesh->mNumVertices, aimesh->mNumFaces, aimesh->mMaterialIndex, aimesh->HasVertexColors(0));

        Mesh::Ref mesh = do_load_mesh(aimesh, want_normals);
        if (!mesh)
            continue;
        return mesh;
    }
    return 0;
}



Mesh::Ref Mesh::load(const char *path, bool want_normals) {
    return load_mesh(path, want_normals);
}
