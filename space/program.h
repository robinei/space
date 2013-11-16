#ifndef SHADER_H
#define SHADER_H

// requires: opengl.h


class Shader : public RefCounted {
public:
	typedef boost::intrusive_ptr<Shader> Ref;

	static Ref create(GLenum type);
	static Ref compile(GLenum type, const char *text);
	static Ref load(GLenum type, const char *path);

	void compile(const char *text);
	void load(const char *path);

	void set_source(const char *text);
	void compile();

	GLenum type();

protected:
	Shader(GLenum type);
	~Shader();

private:
	friend class Program;
	GLenum _type;
	GLuint _shader;
};



class Program : public RefCounted {
public:
	typedef boost::intrusive_ptr<Program> Ref;

	static Ref create();

	void attach(Shader::Ref shader);
	Shader::Ref attached(GLenum type);
	void detach(Shader::Ref shader);
	void detach(GLenum type);
	void detach_all();

	void link();
	
	void bind();
	void unbind();

	GLint attrib_location(const char *name);
	void attrib(const char *name, GLuint index);

	GLint uniform_location(const char *name);
	void uniform(GLint location, GLfloat value);
	void uniform(GLint location, GLint value);
	void uniform(GLint location, const vec3 &value);
	void uniform(GLint location, const vec4 &value);
	void uniform(GLint location, const mat3 &value);
	void uniform(GLint location, const mat4 &value);

	template <typename T>
	void uniform(const char *name, T value) {
		uniform(uniform_location(name), value);
	}

protected:
	Program();
	~Program();

private:
	Program(const Program &);
	Program &operator=(const Program &);

	Shader::Ref &shaderslot(GLenum type);
	Shader::Ref _shaders[6];

	GLuint _program;
};

#endif
