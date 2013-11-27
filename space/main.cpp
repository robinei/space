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
#include <glm/gtc/random.hpp>
#include <glm/gtx/transform.hpp>
#include "opengl.h"

#include "program.h"
#include "bufferobject.h"
#include "mesh.h"
#include "fpscamera.h"
#include "list.h"
#include "quadtree.h"

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





static Program::Ref ship_program;
static Program::Ref qtree_program;
static Mesh::Ref mesh;

static mat4 projection_matrix;
static mat4 view_matrix;
static vec3 light_dir;
static vec3 cursor_pos;





class World;

class Body : public QuadTree::Object {
public:
    vec3 pos;
    vec3 vel;


    ListLink world_link;

    Body() {}

    virtual vec2 qtree_position() {
        return vec2(pos);
    }

    virtual void update(World &world) {}
    virtual void render() {}

private:
    Body(const Body &);
    Body &operator=(const Body &);
};


class World {
public:
    typedef List<Body, &Body::world_link> BodyList;

    BodyList bodies;
    QuadTree quadtree;
    float dt;

    World() : quadtree(Rect(-1000, -1000, 1000, 1000), 6) {
        puts("jalla0");
    }

    ~World() {
        while (!bodies.empty())
            delete bodies.front();
    }

    void add_body(Body *body) {
        bodies.push_back(body);
        quadtree.insert(body);
    }

    void update() {
        for (Body *body : bodies)
            body->update(*this);
    }

    void render() {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        for (Body *body : bodies)
            body->render();
    }

private:
    World(const World &);
    World &operator=(const World &);
};




struct UpdateContext {
    UpdateContext(World &w) : world(w), quadtree(w.quadtree) {}

    World &world;
    QuadTree &quadtree;
    std::vector<QuadTree::Object *> neighbours;
};


class Boid : public Body {
public:
    float maxspeed;
    float maxforce;

    void update(World &world) {
        UpdateContext context(world);

        context.quadtree.query(vec2(pos), 50.0f, context.neighbours);
        //printf("context.neighbours: %d\n", context.neighbours.size());

        vec3 acc(0, 0, 0);
        acc += separation(context) * 2.5f;
        acc += alignment(context);
        acc += cohesion(context);
        acc += seek(cursor_pos) * 0.1f;

        vel += acc;

        pos += vel * world.dt;
        qtree_update();
    }

    vec3 separation(UpdateContext &context) {
        float sep = 20.0f;
        vec3 sum(0, 0, 0);
        int count = 0;
        for (auto obj : context.neighbours) {
            Body *b = static_cast<Body *>(obj);
            if (b == this)
                continue;
            vec3 d = pos - b->pos;
            float len = glm::length(d);
            if (len > sep || len <= 0.00001f)
                continue;
            d = glm::normalize(d);
            d /= len;
            sum += d;
            ++count;
        }
        if (count == 0)
            return vec3(0, 0, 0);
        sum /= (float)count;
        return steer_to(sum);
    }

    vec3 alignment(UpdateContext &context) {
        float neighbordist = 50;
        vec3 sum(0, 0, 0);
        int count = 0;
        for (auto obj : context.neighbours) {
            Body *b = static_cast<Body *>(obj);
            if (b == this)
                continue;
            vec3 d = pos - b->pos;
            float dist = glm::length(d);
            if (dist > neighbordist)
                continue;
            sum += b->vel;
            ++count;
        }
        if (count == 0)
            return vec3(0, 0, 0);
        sum /= (float)count;
        return steer_to(sum);
    }

    vec3 cohesion(UpdateContext &context) {
        float neighbordist = 50;
        vec3 sum(0, 0, 0);
        int count = 0;
        for (auto obj : context.neighbours) {
            Body *b = static_cast<Body *>(obj);
            if (b == this)
                continue;
            vec3 d = pos - b->pos;
            float dist = glm::length(d);
            if (dist > neighbordist)
                continue;
            sum += b->pos;
            ++count;
        }
        if (count == 0)
            return vec3(0, 0, 0);
        sum /= (float)count;
        return seek(sum);
    }

    vec3 seek(vec3 target) {
        return steer_to(target - pos);
    }

    vec3 steer_to(vec3 dir) {
        float len = glm::length(dir);
        if (len < 0.000001f)
            return vec3(0, 0, 0);
        dir *= maxspeed / len;
        vec3 steer = dir - vel;
        if (glm::length(steer) > maxforce)
            steer = glm::normalize(steer) * maxforce;
        return steer;
    }

    void render() {
        mat4 model = glm::translate(pos);
        mat4 vm = view_matrix * model;
        mat4 pvm = projection_matrix * vm;
        mat3 normal = glm::inverseTranspose(mat3(vm));

        ship_program->bind();
        ship_program->uniform("m_pvm", pvm);
        ship_program->uniform("m_vm", vm);
        ship_program->uniform("m_normal", normal);
        ship_program->uniform("light_dir", light_dir);
        ship_program->uniform("mat_ambient", vec4(0.329412f, 0.223529f, 0.027451f, 1.0f));
        ship_program->uniform("mat_diffuse", vec4(0.780392f, 0.568627f, 0.113725f, 1.0f));
        ship_program->uniform("mat_specular", vec4(0.992157f, 0.941176f, 0.807843f, 1.0f));
        ship_program->uniform("mat_shininess", 27.89743616f);

        mesh->bind();
        mesh->render();
        mesh->unbind();

        ship_program->unbind();
    }
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








static void LoadTriangle() {
    //mesh = load_mesh("Shipyard.ply");
    mesh = load_mesh("mauriceh_spaceship_model.ply");

    ship_program = Program::create();
    ship_program->attach(Shader::load(GL_VERTEX_SHADER, "../data/shaders/simple.vert"));
    ship_program->attach(Shader::load(GL_FRAGMENT_SHADER, "../data/shaders/simple.frag"));
    ship_program->attrib("in_pos", 0);
    ship_program->attrib("in_normal", 1);
    ship_program->link();
    ship_program->detach_all();

    qtree_program = Program::create();
    qtree_program->attach(Shader::load(GL_VERTEX_SHADER, "../data/shaders/pos.vert"));
    qtree_program->attach(Shader::load(GL_FRAGMENT_SHADER, "../data/shaders/color.frag"));
    qtree_program->attrib("in_pos", 0);
    qtree_program->link();
    qtree_program->detach_all();
}

static void Render() {
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    mat4 model = mat4();
    mat4 vm = view_matrix * model;
    mat4 pvm = projection_matrix * vm;
    mat3 normal = glm::inverseTranspose(mat3(vm));

    ship_program->bind();
    ship_program->uniform("m_pvm", pvm);
    ship_program->uniform("m_vm", vm);
    ship_program->uniform("m_normal", normal);
    ship_program->uniform("light_dir", light_dir);
    ship_program->uniform("mat_ambient", vec4(0.329412f, 0.223529f, 0.027451f, 1.0f));
    ship_program->uniform("mat_diffuse", vec4(0.780392f, 0.568627f, 0.113725f, 1.0f));
    ship_program->uniform("mat_specular", vec4(0.992157f, 0.941176f, 0.807843f, 1.0f));
    ship_program->uniform("mat_shininess", 27.89743616f);

    mesh->bind();
    mesh->render();
    mesh->unbind();

    ship_program->unbind();
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
    mode.w = 1920, mode.h = 1080;

    SDL_Window *window = SDL_CreateWindow(
        "Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        mode.w, mode.h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
        );
    if (!window)
        die("SDL_CreateWindow() error: %s", SDL_GetError());

    //if (SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP) < 0)
    //	die("SDL_SetWindowFullscreen() error: %s", SDL_GetError());

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
    } catch (const std::exception &e) {
        die("error: %s", e.what());
    }


    auto qtreefmt(VertexFormat::create());
    qtreefmt->add(VertexFormat::Position, 0, 2, GL_FLOAT);
    auto qtreemesh = Mesh::create(GL_LINES, 1);
    qtreemesh->bind();
    auto qtreebuf(BufferObject::create());
    qtreebuf->bind(GL_ARRAY_BUFFER);
    qtreebuf->data(sizeof(vec2)*32000, nullptr, GL_STREAM_DRAW);
    qtreemesh->set_vertex_buffer(0, qtreebuf, qtreefmt);
    qtreemesh->unbind();
    std::vector<vec2> qtreelines;

    //SDL_SetRelativeMouseMode(SDL_TRUE);

    World world;

    for (int i = 0; i < 20; ++i) {
        Boid *b = new Boid();
        b->pos = vec3(glm::diskRand(200.0f), 0.0f);
        b->vel = vec3(glm::diskRand(10.0f), 0.0f);
        b->maxspeed = 50.0f;
        b->maxforce = 10.0f;
        world.add_body(b);
    }

    vec3 camera_pos(0, -5, 20);
    float viewport_radius = 40;
    float aspect_ratio = (float)mode.w / (float)mode.h;

    /*FPSCamera camera;
    camera.set_pos(camera_pos);
    camera.look_at(camera_pos + vec3(0.0f, 1.0f, -2.0f));*/

    light_dir = vec3(-1, 1, -1);

    const Uint8 *keys = SDL_GetKeyboardState(0);
    Uint32 prevticks = SDL_GetTicks();
    bool running = true;

    while (running) {
        Uint32 ticks = SDL_GetTicks();
        Uint32 tickdiff = ticks - prevticks;
        prevticks = ticks;
        float dt = (float)tickdiff / 1000.0f;
        world.dt = dt;

        int mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);

        projection_matrix = glm::ortho(-viewport_radius*aspect_ratio,
                                       viewport_radius*aspect_ratio,
                                       -viewport_radius,
                                       viewport_radius,
                                       -100.0f,
                                       100.0f);
        //projection_matrix = glm::perspective(45.0f, ratio, 0.1f, 100.0f);

        view_matrix = glm::lookAt(camera_pos,
                                  camera_pos + vec3(0.0f, 1.0f, -2.0f),
                                  vec3(0, 0, 1));
        //view_matrix = camera.view_matrix();

        light_dir = glm::normalize(glm::angleAxis(dt*100.0f, vec3(0, 1, 0)) * light_dir);
        light_dir = glm::normalize(glm::angleAxis(dt*10.0f, vec3(1, 0, 0)) * light_dir);

        cursor_pos = glm::unProject(vec3((float)mx, (float)mode.h - (float)my, 0.0f),
                                    view_matrix,
                                    projection_matrix,
                                    vec4(0.0f, 0.0f, (float)mode.w, (float)mode.h));
        cursor_pos.z = 0.0f;

        world.update();
        world.bodies.front();
        world.bodies.front()->pos = cursor_pos;
        world.render();

        qtree_program->bind();
        qtree_program->uniform("m_pvm", projection_matrix * view_matrix);
        qtree_program->uniform("color", vec4(0.2f, 0.2f, 0.2f, 1));
        qtreelines.clear();
        world.quadtree.gather_outlines(qtreelines);
        qtreebuf->bind(GL_ARRAY_BUFFER);
        qtreebuf->write(0, sizeof(qtreelines[0])*qtreelines.size(), &qtreelines[0]);
        qtreebuf->unbind();
        qtreemesh->set_num_vertexes(qtreelines.size());
        qtreemesh->bind();
        qtreemesh->render();
        qtreemesh->unbind();
        qtree_program->unbind();

        //Render();
        SDL_GL_SwapWindow(window);

        float speed = 50.0;
        float sensitivity = 0.01f;

        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] || mx == 0)
            camera_pos.x -= dt*speed;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] || mx == mode.w - 1)
            camera_pos.x += dt*speed;
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] || my == 0)
            camera_pos.y += dt*speed;
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] || my == mode.h - 1)
            camera_pos.y -= dt*speed;
        /*if (keys[SDL_SCANCODE_SPACE])
            camera_pos.y += dt*speed;
        if (keys[SDL_SCANCODE_C])
            camera_pos.y -= dt*speed;*/
        /*if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A])
            camera.strafe(-dt*speed);
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D])
            camera.strafe(dt*speed);
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W])
            camera.step(dt*speed);
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S])
            camera.step(-dt*speed);
        if (keys[SDL_SCANCODE_SPACE])
            camera.rise(dt*speed);
        if (keys[SDL_SCANCODE_C])
            camera.rise(-dt*speed);*/

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
            case SDL_MOUSEWHEEL:
                viewport_radius += -0.2f * event.wheel.y * viewport_radius;
                if (viewport_radius > 200.0f)
                    viewport_radius = 200.0f;
                if (viewport_radius < 20.0f)
                    viewport_radius = 20.0f;
                break;
            case SDL_QUIT:
                running = false;
                break;
            }
        }
    }

    ship_program = 0;
    qtree_program = 0;
    mesh = 0;

    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("Done.\n");
    return 0;
}
