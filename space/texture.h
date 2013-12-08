#ifndef TEXTURE_H
#define TEXTURE_H

// requires: opengl.h
#include "refcounted.h"

class Texture : public RefCounted {
public:
    typedef boost::intrusive_ptr<Texture> Ref;

    static Ref create();
    static Ref create(const char *path);
    static Ref create_cubemap(const char *paths[6]);

    void bind(GLenum texture, GLenum target = GL_TEXTURE_2D);
    void unbind();

    void load(const char *path);

    void load_cubemap(const char *paths[6]);

protected:
    Texture();
    ~Texture();

private:
    Texture(const Texture &);
    Texture &operator=(const Texture &);

    GLenum bound_target;
    GLuint handle;
};

#endif
