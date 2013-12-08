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
#include <glm/gtx/vector_angle.hpp>
#include "opengl.h"

#include "list.h"
#include "pool.h"
#include "program.h"
#include "bufferobject.h"
#include "mesh.h"
#include "texture.h"
#include "statecontext.h"
#include "fpscamera.h"
#include "quadtree.h"
#include "renderqueue.h"
#include "mtrand.h"
#include "ecos.h"

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





static RenderQueue renderqueue;
static Program::Ref ship_program;
static Program::Ref qtree_program;
static Mesh::Ref ship_mesh;
static Mesh::Ref asteroid_mesh;
static float ship_radius;
static float asteroid_radius;

static vec3 cursor_pos;







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


static Mesh::Ref do_load_mesh(aiMesh *aimesh, float &radius_out, bool want_normals) {
    std::vector<GLfloat> verts;
    std::vector<GLuint> indices;

    assert(aimesh->HasPositions());
    if (want_normals) {
        assert(aimesh->HasNormals());
    }

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
        if (len > radius_out)
            radius_out = len;
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

    return mesh;
}

Mesh::Ref load_mesh(const std::string &filename, float &radius_out, bool want_normals=true) {
    Assimp::Importer importer;

    unsigned int flags = aiProcess_Triangulate |
        aiProcess_SortByPType |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes |
        aiProcess_OptimizeGraph |
        aiProcess_PreTransformVertices;
    if (want_normals)
        flags |= aiProcess_GenSmoothNormals;
    const aiScene* scene = importer.ReadFile(filename, flags);

    if (!scene) {
        printf("import error: %s\n", importer.GetErrorString());
        return 0;
    }

    printf("num meshes: %d\n\n", scene->mNumMeshes);

    radius_out = 0;
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh *aimesh = scene->mMeshes[i];
        printf("  %s -\tverts: %d,\tfaces: %d,\tmat: %d,\thas colors: %d\n", aimesh->mName.C_Str(), aimesh->mNumVertices, aimesh->mNumFaces, aimesh->mMaterialIndex, aimesh->HasVertexColors(0));

        Mesh::Ref mesh = do_load_mesh(aimesh, radius_out, want_normals);
        if (!mesh)
            continue;
        return mesh;
    }
    return 0;
}






class SkyBox {
public:
    SkyBox() {
        program = Program::create();
        program->attach(Shader::load(GL_VERTEX_SHADER, "../data/shaders/skybox.vert"));
        program->attach(Shader::load(GL_FRAGMENT_SHADER, "../data/shaders/skybox.frag"));
        program->attrib("in_pos", 0);
        program->link();
        program->detach_all();

        const char *paths[6] {
            "../data/skyboxes/default_right1.jpg",
            "../data/skyboxes/default_left2.jpg",
            "../data/skyboxes/default_top3.jpg",
            "../data/skyboxes/default_bottom4.jpg",
            "../data/skyboxes/default_front5.jpg",
            "../data/skyboxes/default_back6.jpg"
        };
        texture = Texture::create_cubemap(paths);

        float radius;
        mesh = load_mesh("../data/meshes/sphere.ply", radius, false);
        printf("skybox radius: %f\n", radius);
        printf("skybox vertexes: %d\n", mesh->vertex_buffer(0)->size() / (sizeof(float)* 3));
    }

    void render(mat4 view_matrix, mat4 projection_matrix) {
        // remove translation element from view matrix
        // (so we render the mesh centered around the camera)
        view_matrix[3].x = 0;
        view_matrix[3].y = 0;
        view_matrix[3].z = 0;

        StateContext context;
        context.disable(GL_CULL_FACE);
        context.depth_func(GL_ALWAYS);
        context.depth_mask(GL_FALSE);
        
        program->bind();
        program->uniform("m_pvm", projection_matrix * view_matrix);
        program->uniform("tex", 0);
        texture->bind(0, GL_TEXTURE_CUBE_MAP);
        mesh->bind();
        mesh->render_indexed();
        mesh->unbind();
        texture->unbind();
        program->unbind();
    }

private:
    Program::Ref program;
    Texture::Ref texture;
    Mesh::Ref mesh;
};

















template <class T, SystemType Type>
class PoolSystem : public System {
public:
    enum { TYPE = Type };
    SystemType type() override { return TYPE; }

    T *create_component() {
        return pool.create();
    }

    void destroy_component(T *c) {
        pool.free(c);
    }

    typename IterablePool<T>::iterator begin() { return pool.begin(); }
    typename IterablePool<T>::iterator end() { return pool.end(); }

protected:
    IterablePool<T> pool;
};


template <class T, ComponentType Type, class SystemT>
struct PoolComponent : public Component {
    enum { TYPE = Type };
    
    ComponentType type() override { return TYPE; }
    
    static T *create(EntityManager *m) {
        SystemT *sys = m->get_system<SystemT>();
        return sys->create_component();
    }

    void destroy(EntityManager *m) override {
        SystemT *sys = m->get_system<SystemT>();
        sys->destroy_component(static_cast<T *>(this));
    }
};







struct Body :
    public PoolComponent<Body, 'BODY', class BodySystem>,
    public QuadTree::Object
{
    vec3 pos;
    vec3 vel;
    vec3 move;
    float radius;
    Entity *entity;

    void qtree_position(float &x, float &y) override {
        x = pos.x;
        y = pos.y;
    }

    void init(EntityManager *m, Entity *e) override;
};

class BodySystem : public PoolSystem<Body, 'BODY'> {
public:
    BodySystem() : quad_tree(-1000, -1000, 1000, 1000, 8) {}

    QuadTree quad_tree;

    void update(float dt);
};

void Body::init(EntityManager *m, Entity *e) {
    BodySystem *sys = m->get_system<BodySystem>();
    sys->quad_tree.insert(this);
    entity = e;
}









vec3 limit(vec3 v, float len) {
    if (glm::length(v) > len)
        return glm::normalize(v) * len;
    return v;
}


struct Ship : public PoolComponent<Ship, 'SHIP', class ShipSystem> {
    vec3 dir;
    float maxspeed;
    float maxforce;
    int team;
    Body *body;

    enum { MAX_FRIENDS = 4 };
    Entity *friends[MAX_FRIENDS];
    float friend_radius;

    enum { MAX_CLOSEST = 4 };
    Entity *closest[MAX_CLOSEST];
    float closest_radius;

    Ship() {
        friend_radius = 50;
        closest_radius = 50;
    }

    void init(EntityManager *m, Entity *e) override;



    void update(EntityManager *m, float dt);

    vec3 planehug() {
        vec3 target = body->pos;
        target.z = 0;
        return arrive(target);
    }

    vec3 zseparation() {
        float sep = 20.0f;
        vec3 sum(0, 0, 0);
        int count = 0;
        for (int i = 0; i < MAX_CLOSEST; ++i) {
            Entity *e = closest[i];
            if (!e) continue;

            Body *b = e->get_component<Body>();
            vec3 d = body->pos - b->pos;
            float len = glm::length(d);
            if (len > sep || len <= 0.00001f) continue;
            //d = glm::normalize(d);
            float dz = d.z;
            if (dz == 0.0f)
                dz = glm::dot(glm::normalize(body->vel), glm::normalize(b->vel));
            dz /= fabsf(dz);
            dz /= len;
            sum += vec3(0, 0, dz);
            ++count;
        }
        if (count == 0)
            return vec3(0, 0, 0);
        sum /= (float)count;
        return steer(sum);
    }

    vec3 separation() {
        float sep = 20.0f;
        vec3 sum(0, 0, 0);
        int count = 0;
        for (int i = 0; i < MAX_CLOSEST; ++i) {
            Entity *e = closest[i];
            if (!e) continue;

            Body *b = e->get_component<Body>();
            vec3 d = body->pos - b->pos;
            d.z = 0;
            float len = glm::length(d);
            if (len > sep || len <= 0.00001f) continue;
            d = glm::normalize(d);
            d /= len;
            sum += d;
            ++count;
        }
        if (count == 0)
            return vec3(0, 0, 0);
        sum /= (float)count;
        return steer(sum);
    }

    vec3 alignment() {
        float neighbordist = 50;
        vec3 sum(0, 0, 0);
        int count = 0;
        for (int i = 0; i < MAX_FRIENDS; ++i) {
            Entity *e = friends[i];
            if (!e) continue;

            Body *b = e->get_component<Body>();
            Ship *s = b->entity->get_component<Ship>();
            if (s->team != team) continue;
            
            vec3 d = body->pos - b->pos;
            float dist = glm::length(d);
            if (dist > neighbordist) continue;
            sum += b->vel;
            ++count;
        }
        if (count == 0)
            return vec3(0, 0, 0);
        sum /= (float)count;
        return steer(sum);
    }

    vec3 cohesion() {
        float neighbordist = 50;
        vec3 sum(0, 0, 0);
        int count = 0;
        for (int i = 0; i < MAX_FRIENDS; ++i) {
            Entity *e = friends[i];
            if (!e) continue;

            Body *b = e->get_component<Body>();
            Ship *s = b->entity->get_component<Ship>();
            if (s->team != team) continue;
            
            vec3 d = body->pos - b->pos;
            float len = glm::length(d);
            if (len > neighbordist) continue;
            sum += b->pos;
            ++count;
        }
        if (count == 0)
            return vec3(0, 0, 0);
        sum /= (float)count;
        return seek(sum);
    }

    vec3 seek(vec3 target) {
        return steer(target - body->pos);
    }

    vec3 steer(vec3 dir) {
        float len = glm::length(dir);
        if (len < 0.000001f)
            return vec3(0, 0, 0);
        dir *= maxspeed / len;
        return limit(dir - body->vel, maxforce);
    }

    vec3 arrive(vec3 target) {
        float brakelimit = 10.0f;
        vec3 desired = target - body->pos;
        float len = glm::length(desired);
        if (len < 0.000001f)
            return vec3(0, 0, 0);
        desired /= len;
        if (len < brakelimit) {
            desired *= (len / brakelimit) * maxspeed;
        } else {
            desired *= maxspeed;
        }
        return limit(desired, maxforce);
    }

    mat4 calc_rotation_matrix() {
        vec3 up(0, 0, 1);
        vec3 forward(glm::normalize(dir));
        vec3 right(glm::cross(forward, up));
        up = glm::cross(right, forward);
        right = glm::normalize(glm::cross(forward, up));
        up = glm::normalize(glm::cross(right, forward));

        mat4 m;
        m[0] = vec4(right, 0);
        m[1] = vec4(forward, 0);
        m[2] = vec4(up, 0);
        return m;
    }
};

class ShipSystem : public PoolSystem<Ship, 'SHIP'> {
public:
    void update(EntityManager *m, float dt);
};

void Ship::init(EntityManager *m, Entity *e) {
    body = e->get_component<Body>();
    assert(body);
}

void ShipSystem::update(EntityManager *m, float dt) {
    for (Ship *ship : *this) {
        ship->update(m, dt);
    }
}




class SimpleRenderable : public PoolComponent<SimpleRenderable, 'SRND', class SimpleRenderableSystem> {
public:
    mat4 model_matrix;

    vec4 ambient_color;
    vec4 diffuse_color;
    vec4 specular_color;
    float shininess;

    Program::Ref program;
    Mesh::Ref mesh;
};

class SimpleRenderableSystem : public PoolSystem<SimpleRenderable, 'SRND'> {
public:
    void render(RenderQueue *renderqueue, mat4 view_matrix, mat4 projection_matrix) {
        for (SimpleRenderable *r : *this) {
            mat4 vm = view_matrix * r->model_matrix;
            mat4 pvm = projection_matrix * vm;
            mat3 normal = glm::inverseTranspose(mat3(vm));

            auto cmd = renderqueue->add_command(r->program, r->mesh);
            cmd->add_uniform("m_pvm", pvm);
            cmd->add_uniform("m_vm", vm);
            cmd->add_uniform("m_normal", normal);
            cmd->add_uniform("mat_ambient", r->ambient_color);
            cmd->add_uniform("mat_diffuse", r->diffuse_color);
            cmd->add_uniform("mat_specular", r->specular_color);
            cmd->add_uniform("mat_shininess", r->shininess);
        }
    }
};

/*
class Doodad : public PoolComponent<Doodad, 'DOOD', class DoodadSystem> {
public:
};

class DoodadSystem : public PoolSystem<Doodad, 'DOOD'> {
    void update() {

    }
};
*/

void BodySystem::update(float dt) {
    /*for (Body *b : *this) {
        Entity *e = b->entity;
        Ship *s = e->get_component<Ship>();
        if (!s)
            continue;

        for (int i = 0; i < Ship::MAX_CLOSEST; ++i) {
            Entity *e2 = s->closest[i];
            if (!e2)
                continue;
            Body *b2 = e2->get_component<Body>();
            if (!b2)
                continue;

            vec3 d = b2->pos - b->pos;
            float radius = b->radius + b2->radius;
            if (d.x*d.x + d.y*d.y > radius*radius)
                continue;

            puts("collision");
            Plane plane(glm::normalize(d));
            b->move = plane.project(b->move);
            //b2->move = plane.project(b2->move);
        }
    }*/
    for (Body *b : *this) {
        b->pos += b->move;
    }
}

static float adjust_query_radius(float radius, int num_found, int maximum) {
    if (num_found < maximum) radius += 0.1f;
    else if (num_found > maximum) radius -= 0.1f;
    return clamp(radius, 1.0f, 50.0f);
}

void Ship::update(EntityManager *m, float dt) {
    BodySystem *sys = m->get_system<BodySystem>();
    Entity *entity = body->entity;

    int num_friends = 0;
    int num_closest = 0;
    memset(friends, 0, sizeof(friends));
    memset(closest, 0, sizeof(closest));

    float friend_radius_squared = friend_radius*friend_radius;
    float closest_radius_squared = closest_radius*closest_radius;
    float query_radius = std::max(friend_radius, closest_radius);
    
    vec2 p(body->pos);

    sys->quad_tree.query(p.x - query_radius, p.y - query_radius,
                         p.x + query_radius, p.y + query_radius,
                         [&](QuadTree::Object *obj) mutable
    {
        Body *b = static_cast<Body *>(obj);
        if (b == body)
            return;
        vec2 d = vec2(b->pos) - p;
        float dist_squared = d.x*d.x + d.y*d.y;

        if (dist_squared <= friend_radius_squared) {
            Ship *s = b->entity->get_component<Ship>();
            if (s && s->team == team) {
                if (num_friends < MAX_FRIENDS)
                    friends[num_friends] = b->entity;
                num_friends++;
            }
        }

        if (dist_squared <= closest_radius_squared) {
            if (num_closest < MAX_CLOSEST)
                closest[num_closest] = b->entity;
            num_closest++;
        }
    });

    friend_radius = adjust_query_radius(friend_radius, num_friends, MAX_FRIENDS);
    closest_radius = adjust_query_radius(closest_radius, num_closest, MAX_CLOSEST);



    vec3 acc(0, 0, 0);

    acc += separation() * 1.5f;
    acc += alignment() * 1.0f;
    acc += cohesion() * 1.0f;

    acc += planehug() * 1.5f;
    acc += zseparation() * 1.5f;

    acc += seek(cursor_pos) * 1.0f;

    body->vel += acc * dt;
    body->vel = limit(body->vel, maxspeed);

    body->move = body->vel * dt;
    //pos.z = 0;

    body->qtree_update();

    vec3 v = glm::normalize(body->vel);
    float a = glm::angle(dir, v);
    if (fabsf(a) > 0.001f) {
        vec3 axis(glm::cross(dir, v));
        dir = glm::normalize(dir * glm::angleAxis(glm::min(45.0f * dt, a), axis));
    }


    SimpleRenderable *r = entity->get_component<SimpleRenderable>();
    if (r) {
        r->model_matrix = glm::translate(body->pos) * calc_rotation_matrix();
    }
}







static void do_spawn_boid(EntityManager *m, vec3 pos) {
    Entity *e = m->create_entity();
    
    Body *b = m->add_component<Body>(e);
    b->pos = pos;
    b->vel = vec3(glm::diskRand(10.0f), 0.0f);
    b->radius = ship_radius;

    Ship *s = m->add_component<Ship>(e);
    s->dir = glm::normalize(b->vel);
    s->maxspeed = 40;
    s->maxforce = 1;
    s->team = rand() % 2;
    
    SimpleRenderable *r = m->add_component<SimpleRenderable>(e);
    r->mesh = ship_mesh;
    r->program = ship_program;
    if (s->team == 0) {
        r->ambient_color = vec4(0.25f, 0.25f, 0.25f, 1);
        r->diffuse_color = vec4(0.4f, 0.4f, 0.4f, 1);
        r->specular_color = vec4(0.774597f, 0.774597f, 0.774597f, 1);
        r->shininess = 76.8f;
    } else {
        r->ambient_color = vec4(0.329412f, 0.223529f, 0.027451f, 1.0f);
        r->diffuse_color = vec4(0.780392f, 0.568627f, 0.113725f, 1.0f);
        r->specular_color = vec4(0.992157f, 0.941176f, 0.807843f, 1.0f);
        r->shininess = 27.89743616f;
    }

    m->optimize_entity(e);
    m->init_entity(e);
}

static void add_asteroid(EntityManager *m, vec3 pos) {
    Entity *e = m->create_entity();

    Body *b = m->add_component<Body>(e);
    b->pos = pos;
    b->radius = asteroid_radius;

    SimpleRenderable *r = m->add_component<SimpleRenderable>(e);
    r->model_matrix = glm::translate(pos) * glm::scale(vec3(10, 10, 10));
    r->mesh = asteroid_mesh;
    r->program = ship_program;
    r->ambient_color = vec4(0.25f, 0.25f, 0.25f, 1);
    r->diffuse_color = vec4(0.4f, 0.4f, 0.4f, 1);
    r->specular_color = vec4(0.774597f, 0.774597f, 0.774597f, 1);
    //r->shininess = 76.8f;
    r->shininess = 160.8f;

    m->optimize_entity(e);
    m->init_entity(e);
}


















SDL_DisplayMode mode;
mat4 projection_matrix, view_matrix;

static vec3 screen_to_world(int x, int y) {
    vec3 p0 = glm::unProject(vec3(x, mode.h - y - 1, 0), view_matrix, projection_matrix, vec4(0, 0, mode.w, mode.h));
    vec3 p1 = glm::unProject(vec3(x, mode.h - y - 1, 1), view_matrix, projection_matrix, vec4(0, 0, mode.w, mode.h));
    return Plane::XY().ray_intersect(p0, p1);
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
    ok = ok && SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24) == 0;
    ok = ok && SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1) == 0;
    ok = ok && SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4) == 0;
    ok = ok && SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1) == 0;
    if (!ok)
        die("SDL_GL_SetAttribute() error: %s", SDL_GetError());

    if (SDL_GetDesktopDisplayMode(0, &mode) < 0)
        die("SDL_GetDesktopDisplayMode() error: %s", SDL_GetError());
    //mode.w = 1920, mode.h = 1080;

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

    StateContext context;
    context.enable(GL_MULTISAMPLE);

    try {
        ship_mesh = load_mesh("../data/meshes/harv.ply", ship_radius);
        asteroid_mesh = load_mesh("../data/meshes/asteroid.ply", asteroid_radius);
        asteroid_radius *= 10;
        printf("ship_radius: %f\n", ship_radius);
        printf("asteroid_radius: %f\n", asteroid_radius);
    } catch (const std::exception &e) {
        die("error: %s", e.what());
    }

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

    std::vector<vec2> qtree_lines;
    const int qtree_max_vertexes = 16000;
    auto qtree_fmt(VertexFormat::create());
    qtree_fmt->add(VertexFormat::Position, 0, 2, GL_FLOAT);
    
    auto qtree_buf(BufferObject::create());
    qtree_buf->bind();
    qtree_buf->data(sizeof(vec2)*qtree_max_vertexes, nullptr, GL_STREAM_DRAW);
    qtree_buf->unbind();

    auto qtree_mesh = Mesh::create(GL_LINES, 1);
    qtree_mesh->set_vertex_buffer(0, qtree_buf, qtree_fmt);


    std::vector<vec3> vlines_lines;
    const int vlines_max_vertexes = 16000;
    auto vlines_fmt(VertexFormat::create());
    vlines_fmt->add(VertexFormat::Position, 0, 3, GL_FLOAT);
    
    auto vlines_buf(BufferObject::create());
    vlines_buf->bind();
    vlines_buf->data(sizeof(vec2)*vlines_max_vertexes, nullptr, GL_STREAM_DRAW);
    vlines_buf->unbind();

    auto vlines_mesh = Mesh::create(GL_LINES, 1);
    vlines_mesh->set_vertex_buffer(0, vlines_buf, vlines_fmt);

    //SDL_SetRelativeMouseMode(SDL_TRUE);


    BodySystem body_system;
    ShipSystem ship_system;
    SimpleRenderableSystem simple_renderable_system;
    EntityManager entity_manager;
    entity_manager.add_system(&body_system);
    entity_manager.add_system(&ship_system);
    entity_manager.add_system(&simple_renderable_system);
    entity_manager.optimize_systems();

    for (int i = 0; i < 40; ++i) {
        do_spawn_boid(&entity_manager, vec3(glm::diskRand(200.0f), 0.0f));
    }
    for (int i = 0; i < 10; ++i) {
        add_asteroid(&entity_manager, vec3(glm::diskRand(400.0f), 0.0f));
    }

    vec3 camera_focus(0, 0, 0);
    float camera_dist = 100;
    vec3 camera_dir = glm::normalize(vec3(1, 1, 1));
    float aspect_ratio = (float)mode.w / (float)mode.h;
    bool orthogonal_projection = false;
    vec3 light_dir = glm::normalize(vec3(1, 0, 3));

    const Uint8 *keys = SDL_GetKeyboardState(0);
    Uint32 prevticks = SDL_GetTicks();
    bool running = true;
    bool rotating = false;

    SkyBox skybox;

    while (running) {
        //////////////////////////////////////////////////////////////////////////////////////////////////
        // Updating:
        //////////////////////////////////////////////////////////////////////////////////////////////////

        Uint32 ticks = SDL_GetTicks();
        Uint32 tickdiff = ticks - prevticks;
        prevticks = ticks;
        float dt = (float)tickdiff / 1000.0f;

        int mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);

        vec3 camera_pos = camera_focus + camera_dir * camera_dist;
        
        vec3 camera_forward = -camera_dir;
        camera_forward.z = 0;
        camera_forward = glm::normalize(camera_forward);

        vec3 camera_right = glm::cross(vec3(0, 0, 1), camera_forward);

        if (orthogonal_projection) {
            float dim = camera_pos.z;
            projection_matrix = glm::ortho(-dim*aspect_ratio, dim*aspect_ratio,
                                           -dim, dim,
                                           -10000.0f, 10000.0f);
        } else {
            projection_matrix = glm::perspective(45.0f, aspect_ratio, 0.1f, 10000.0f);
        }

        view_matrix = glm::lookAt(camera_pos,
                                  camera_focus,
                                  vec3(0, 0, 1));

        vec3 screen_center = screen_to_world(mode.w / 2, mode.h / 2);
        cursor_pos = screen_to_world(mx, my);

        //light_dir = glm::normalize(glm::angleAxis(dt*10.0f, vec3(0, 0, 1)) * light_dir);

        ship_system.update(&entity_manager, dt);
        body_system.update(dt);
        entity_manager.update();


        //////////////////////////////////////////////////////////////////////////////////////////////////
        // Rendering:
        //////////////////////////////////////////////////////////////////////////////////////////////////

        ship_program->bind();
        ship_program->uniform("light_dir", light_dir);
        ship_program->unbind();

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        context.enable(GL_DEPTH_TEST);
        context.depth_func(GL_LESS);
        context.enable(GL_CULL_FACE);
        context.cull_face(GL_BACK);

        skybox.render(view_matrix, projection_matrix);

        {
            qtree_lines.clear();
            body_system.quad_tree.gather_outlines([&](float x, float y) mutable {
                qtree_lines.push_back(vec2(x, y));
            });
            if (qtree_lines.size() > qtree_max_vertexes)
                qtree_lines.resize(qtree_max_vertexes);
            qtree_buf->bind();
            qtree_buf->write(0, sizeof(qtree_lines[0])*qtree_lines.size(), &qtree_lines[0]);
            qtree_buf->unbind();
            qtree_mesh->set_num_vertexes(qtree_lines.size());

            auto cmd = renderqueue.add_command(qtree_program, qtree_mesh);
            cmd->indexed = false;
            cmd->add_uniform("m_pvm", projection_matrix * view_matrix);
            cmd->add_uniform("color", vec4(0.2f, 0.2f, 0.2f, 1));

            StateContext context;
            context.depth_mask(GL_FALSE);
            renderqueue.flush();
        }

        {
            vlines_lines.clear();
            for (auto b : body_system) {
                vec3 pos = b->pos;
                pos.z = 0;
                vlines_lines.push_back(pos);
                vlines_lines.push_back(b->pos);
            }
            if (vlines_lines.size() > vlines_max_vertexes)
                vlines_lines.resize(vlines_max_vertexes);
            vlines_buf->bind();
            vlines_buf->write(0, sizeof(vlines_lines[0])*vlines_lines.size(), &vlines_lines[0]);
            vlines_buf->unbind();
            vlines_mesh->set_num_vertexes(vlines_lines.size());

            auto cmd = renderqueue.add_command(qtree_program, vlines_mesh);
            cmd->indexed = false;
            cmd->add_uniform("m_pvm", projection_matrix * view_matrix);
            cmd->add_uniform("color", vec4(0.5f, 0.5f, 0.5f, 1));
            renderqueue.flush();
        }

        simple_renderable_system.render(&renderqueue, view_matrix, projection_matrix);
        renderqueue.flush();
        
        SDL_GL_SwapWindow(window);


        //////////////////////////////////////////////////////////////////////////////////////////////////
        // Event handling:
        //////////////////////////////////////////////////////////////////////////////////////////////////

        float speed = 1.0;
        float sensitivity = 0.01f;

        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] || (mx == 0 && !rotating))
            camera_focus += camera_right*camera_pos.z*dt*speed;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] || (mx == mode.w - 1 && !rotating))
            camera_focus -= camera_right*camera_pos.z*dt*speed;
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] || (my == 0 && !rotating))
            camera_focus += camera_forward*camera_pos.z*dt*speed;
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] || (my == mode.h - 1 && !rotating))
            camera_focus -= camera_forward*camera_pos.z*dt*speed;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_KEYDOWN:
                break;
            case SDL_KEYUP:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = false;
                if (event.key.keysym.sym == SDLK_p)
                    orthogonal_projection = !orthogonal_projection;
                break;
            case SDL_MOUSEMOTION:
                if (rotating) {
                    ;
                    float angle_h = -360.f * (float)event.motion.xrel / (float)mode.w;
                    camera_dir = glm::angleAxis(angle_h, vec3(0, 0, 1)) * camera_dir;

                    float angle_v = 360.f * (float)event.motion.yrel / (float)mode.h;
                    camera_dir = glm::angleAxis(angle_v, camera_right) * camera_dir;

                    camera_dir = glm::normalize(camera_dir);
                }
                break;
            case SDL_MOUSEWHEEL: {
                ;
                camera_dist -= (camera_dist * 0.4f * event.wheel.y);
                camera_dist = clamp(camera_dist, 1.0f, 1000.0f);
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    do_spawn_boid(&entity_manager, cursor_pos);
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (!rotating) {
                        rotating = true;
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                    }
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (rotating) {
                        rotating = false;
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                    }
                }
                break;
            case SDL_QUIT:
                running = false;
                break;
            }
        }
    }

    ship_program = 0;
    qtree_program = 0;
    ship_mesh = 0;

    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("Done.\n");
    return 0;
}
