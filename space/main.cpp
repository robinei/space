#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <cstdint>


#include <SDL.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/transform.hpp>
#include "opengl.h"

#include "program.h"
#include "bufferobject.h"
#include "mesh.h"
#include "fpscamera.h"

#define STBI_HEADER_FILE_ONLY
#include "stb_image.c"


#ifdef WIN32
#include <Windows.h>
#endif

#define die(fmt, ...) \
do { \
\
	printf(fmt "\n", __VA_ARGS__); \
	exit(1); \
} while (0)





class Model : public RefCounted {
public:
};

class Node : public RefCounted {
public:
};





class Body {
public:
	vec3 pos;
	vec3 dir;
};


class QuadTree {
public:
	QuadTree(float minx, float miny, float maxx, float maxy, int depth) : depth(depth) {
		num_nodes = 1;
		int nlevel = 1;
		int d = depth;
		while (d--) {
			nlevel *= 4;
			num_nodes += nlevel;
		}
		nodes = new Node[num_nodes];
		node_index = 0;
		subdiv(minx, miny, maxx, maxy, 0);
	}

	~QuadTree() {
		delete[] nodes;
	}

private:
	struct Node {
		float minx, miny, maxx, maxy;
		Node *child[4];
	};

	Node *subdiv(float minx, float miny, float maxx, float maxy, int curr_depth) {
		Node *node = &nodes[node_index++];
		node->minx = minx;
		node->miny = miny;
		node->maxx = maxx;
		node->maxy = maxy;
		if (curr_depth == depth) {
			node->child[0] = 0;
			node->child[1] = 0;
			node->child[2] = 0;
			node->child[3] = 0;
		}
		else {
			float w = (maxx - minx) / 2.0f;
			float h = (maxy - miny) / 2.0f;
			node->child[0] = subdiv(minx, miny, minx + w, miny + h, curr_depth + 1);
			node->child[1] = subdiv(minx + w, miny, maxx, miny + h, curr_depth + 1);
			node->child[2] = subdiv(minx, miny + h, minx + w, maxy, curr_depth + 1);
			node->child[3] = subdiv(minx + w, miny + h, maxx, maxy, curr_depth + 1);

		}
		return node;
	}

	int node_index;
	int depth;
	int num_nodes;
	Node *nodes;
};






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


static Mesh::Ref do_load_mesh(aiMesh *aimesh) {
	std::vector<GLfloat> verts;
	std::vector<GLuint> indices;

	assert(aimesh->HasPositions());
	assert(aimesh->HasNormals());

	for (unsigned int i = 0; i < aimesh->mNumVertices; ++i) {
		aiVector3D v = aimesh->mVertices[i];
		verts.push_back(v.x);
		verts.push_back(v.y);
		verts.push_back(v.z);

		aiVector3D n = aimesh->mNormals[i];
		verts.push_back(n.x);
		verts.push_back(n.y);
		verts.push_back(n.z);
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
	format->add(VertexFormat::Normal, 1, 3, GL_FLOAT);

	Mesh::Ref mesh = Mesh::create(GL_TRIANGLES, 1);
	mesh->bind();

	BufferObject::Ref index_buffer = BufferObject::create();
	index_buffer->bind(GL_ELEMENT_ARRAY_BUFFER);
	index_buffer->data(sizeof(indices[0])*indices.size(), &indices[0]);
	mesh->set_index_buffer(index_buffer, indices.size(), GL_UNSIGNED_INT);

	BufferObject::Ref vertex_buffer = BufferObject::create();
	vertex_buffer->bind(GL_ARRAY_BUFFER);
	vertex_buffer->data(sizeof(verts[0])*verts.size(), &verts[0]);
	mesh->set_vertex_buffer(0, vertex_buffer, format);

	mesh->unbind();
	return mesh;
}

Mesh::Ref load_mesh(const std::string &filename) {
	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(filename,
		aiProcess_Triangulate |
		aiProcess_SortByPType |
		aiProcess_JoinIdenticalVertices |
		aiProcess_OptimizeMeshes |
		aiProcess_OptimizeGraph |
		aiProcess_PreTransformVertices |
		aiProcess_GenSmoothNormals
	);

	if (!scene) {
		printf("import error: %s\n", importer.GetErrorString());
		return 0;
	}

	printf("num meshes: %d\n\n", scene->mNumMeshes);

	for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
		aiMesh *aimesh = scene->mMeshes[i];
		printf("  %s -\tverts: %d,\tfaces: %d,\tmat: %d,\thas colors: %d\n", aimesh->mName.C_Str(), aimesh->mNumVertices, aimesh->mNumFaces, aimesh->mMaterialIndex, aimesh->HasVertexColors(0));

		Mesh::Ref mesh = do_load_mesh(aimesh);
		if (!mesh)
			continue;
		return mesh;
	}
	return 0;
}











static Program::Ref program;
static Mesh::Ref mesh;

static mat4 projection_matrix;
static mat4 view_matrix;
static vec3 light_dir;

static void LoadTriangle() {
	//mesh = load_mesh("Shipyard.ply");
	mesh = load_mesh("mauriceh_spaceship_model.ply");

	program = Program::create();
	program->attach(Shader::load(GL_VERTEX_SHADER, "../data/shaders/simple.vert"));
	program->attach(Shader::load(GL_FRAGMENT_SHADER, "../data/shaders/simple.frag"));
	program->attrib("in_pos", 0);
	program->attrib("in_normal", 1);
	program->link();
	program->detach_all();
}

static void Render() {
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	mat4 model = mat4();
	mat4 vm = view_matrix * model;
	mat4 pvm = projection_matrix * vm;
	mat3 normal = glm::inverseTranspose(mat3(vm));

	program->bind();
	program->uniform("m_pvm", pvm);
	program->uniform("m_vm", vm);
	program->uniform("m_normal", normal);
	program->uniform("light_dir", light_dir);
	program->uniform("mat_ambient", vec4(0.329412f, 0.223529f, 0.027451f, 1.0f));
	program->uniform("mat_diffuse", vec4(0.780392f, 0.568627f, 0.113725f, 1.0f));
	program->uniform("mat_specular", vec4(0.992157f, 0.941176f, 0.807843f, 1.0f));
	program->uniform("mat_shininess", 27.89743616f);

	mesh->bind();
	mesh->render();
	mesh->unbind();

	program->unbind();
}




int main(int argc, char *argv[]) {
#ifdef WIN32
	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		freopen("CON", "w", stdout);
		freopen("CON", "w", stderr);
	}
#endif
	printf("Starting...\n");

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		die("SDL_Init() error: %s", SDL_GetError());

	bool ok = true;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) == 0;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2) == 0;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) == 0;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) == 0;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8) == 0;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8) == 0;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8) == 0;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8) == 0;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16) == 0;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1) == 0;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8) == 0;
	ok = ok && SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1) == 0;
	if (!ok)
		die("SDL_GL_SetAttribute() error: %s", SDL_GetError());

	SDL_DisplayMode mode;
	if (SDL_GetDesktopDisplayMode(0, &mode) < 0)
		die("SDL_GetDesktopDisplayMode() error: %s", SDL_GetError());

	SDL_Window *window = SDL_CreateWindow(
		"Test",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		mode.w, mode.h,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
	);
	if (!window)
		die("SDL_CreateWindow() error: %s", SDL_GetError());

	if (SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP) < 0)
		die("SDL_SetWindowFullscreen() error: %s", SDL_GetError());

	SDL_GLContext glcontext = SDL_GL_CreateContext(window);
	if (!glcontext)
		die("SDL_GL_CreateContext() error: %s", SDL_GetError());

	glewExperimental = GL_TRUE;
	GLenum status = glewInit();
	if (status != GLEW_OK)
		die("glewInit() error: %s", glewGetErrorString(status));

	if (!GLEW_VERSION_3_2)
		die("OpenGL 3.2 API is not available.");


	if (SDL_GL_SetSwapInterval(-1) < 0) {
		printf("SDL_GL_SetSwapInterval(-1) failed: %s\n", SDL_GetError());
		if (SDL_GL_SetSwapInterval(1) < 0)
			printf("SDL_GL_SetSwapInterval(1) failed: %s\n", SDL_GetError());
	}

	glEnable(GL_MULTISAMPLE);

	try {
		LoadTriangle();
	}
	catch (const std::exception &e) {
		die("error: %s", e.what());
	}

	Uint32 prevticks = SDL_GetTicks();
	const Uint8 *keys = SDL_GetKeyboardState(0);
	//SDL_SetRelativeMouseMode(SDL_TRUE);

	float ratio = (float)mode.w / (float)mode.h;
	//projection_matrix = glm::perspective(45.0f, ratio, 0.1f, 100.0f);
	float f = 40.0f; projection_matrix = glm::ortho(-f*ratio, f*ratio, -f, f, -100.0f, 100.0f);

	/*FPSCamera camera;
	camera.set_pos(glm::vec3(4, 3, 3));
	camera.look_at(glm::vec3(0, 0, 0));*/
	vec3 camera_pos(0, 20, 5);
	
	light_dir = vec3(-1, -1, 1);
	
	bool running = true;
	while (running) {
		Uint32 ticks = SDL_GetTicks();
		Uint32 tickdiff = ticks - prevticks;
		prevticks = ticks;
		float dt = (float)tickdiff / 1000.0f;

		//view_matrix = camera.view_matrix();
		view_matrix = glm::lookAt(camera_pos,
			camera_pos + vec3(0.0f, -2.0f, -1.0f),
			vec3(0, 1, 0));

		light_dir = glm::normalize(glm::angleAxis(dt*100.0f, vec3(0, 1, 0)) * light_dir);
		light_dir = glm::normalize(glm::angleAxis(dt*10.0f, vec3(1, 0, 0)) * light_dir);

		Render();
		SDL_GL_SwapWindow(window);

		float speed = 10.0;
		float sensitivity = 0.01f;

		int mx=0, my=0;
		SDL_GetMouseState(&mx, &my);
		
		if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] || mx == 0) {
			camera_pos.x -= dt*speed;
		}
		if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] || mx == mode.w-1) {
			camera_pos.x += dt*speed;
		}
		if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] || my == 0) {
			camera_pos.z -= dt*speed;
		}
		if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] || my == mode.h - 1) {
			camera_pos.z += dt*speed;
		}
		/*if (keys[SDL_SCANCODE_SPACE]) {
			camera_pos.y += dt*speed;
		}
		if (keys[SDL_SCANCODE_C]) {
			camera_pos.y -= dt*speed;
		}*/
		/*if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
			camera.strafe(-dt*speed);
		}
		if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
			camera.strafe(dt*speed);
		}
		if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) {
			camera.step(dt*speed);
		}
		if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) {
			camera.step(-dt*speed);
		}
		if (keys[SDL_SCANCODE_SPACE]) {
			camera.rise(dt*speed);
		}
		if (keys[SDL_SCANCODE_C]) {
			camera.rise(-dt*speed);
		}*/

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_KEYDOWN:
				break;
			case SDL_KEYUP:
				if (event.key.keysym.sym == SDLK_ESCAPE)
					running = false;
				break;
			case SDL_MOUSEMOTION:
				//camera.rotate((float)event.motion.xrel * -sensitivity, (float)event.motion.yrel * -sensitivity);
				break;
			case SDL_QUIT:
				running = false;
				break;
			}
		}
	}

	program = 0;
	mesh = 0;

	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);
	SDL_Quit();
	printf("Done.\n");
	return 0;
}
