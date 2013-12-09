#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>

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
#include "skybox.h"

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
static Mesh::Ref ship_mesh;
static Mesh::Ref asteroid_mesh;

static vec3 cursor_pos;


















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

    acc += arrive(cursor_pos) * 1.0f;

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
    b->radius = ship_mesh->radius();

    Ship *s = m->add_component<Ship>(e);
    s->dir = glm::normalize(b->vel);
    s->maxspeed = glm::linearRand(30.0f, 60.0f);
    s->maxforce = glm::linearRand(0.5f, 2.0f);
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
    b->radius = asteroid_mesh->radius() * 10;

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
        ship_mesh = Mesh::load("../data/meshes/harv.ply");
        asteroid_mesh = Mesh::load("../data/meshes/asteroid.ply");
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

    Program::Ref line_program = Program::create();
    line_program->attach(Shader::load(GL_VERTEX_SHADER, "../data/shaders/color.vert"));
    line_program->attach(Shader::load(GL_FRAGMENT_SHADER, "../data/shaders/color.frag"));
    line_program->attrib("in_pos", 0);
    line_program->attrib("in_color", 1);
    line_program->link();
    line_program->detach_all();

#pragma pack(push, 1)
    struct LineVertex {
        vec3 pos;
        vec4 color;
        LineVertex() {}
        LineVertex(vec3 pos, vec4 color) : pos(pos), color(color) {}
    };
#pragma pack(pop)
    std::vector<LineVertex> line_vertexes;
    const int max_line_vertexes = 32000;
    auto line_fmt(VertexFormat::create());
    line_fmt->add(VertexFormat::Position, 0, 3, GL_FLOAT);
    line_fmt->add(VertexFormat::Color, 1, 4, GL_FLOAT);
    
    auto line_buf(BufferObject::create());
    line_buf->bind();
    line_buf->data(sizeof(LineVertex)*max_line_vertexes, nullptr, GL_STREAM_DRAW);
    line_buf->unbind();

    auto line_mesh = Mesh::create(GL_LINES, 1);
    line_mesh->set_vertex_buffer(0, line_buf, line_fmt);


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
    float camera_pitch = 45;
    float camera_yaw = 45;
    float aspect_ratio = (float)mode.w / (float)mode.h;
    bool orthogonal_projection = false;
    vec3 light_dir = glm::normalize(vec3(1, 0, 3));

    const Uint8 *keys = SDL_GetKeyboardState(0);
    Uint32 prevticks = SDL_GetTicks();
    bool running = true;
    bool rotating = false;

    const char *paths[6] {
        "../data/skyboxes/default_right1.jpg",
        "../data/skyboxes/default_left2.jpg",
        "../data/skyboxes/default_top3.jpg",
        "../data/skyboxes/default_bottom4.jpg",
        "../data/skyboxes/default_front5.jpg",
        "../data/skyboxes/default_back6.jpg"
    };
    SkyBox skybox(paths);

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

        if (orthogonal_projection && camera_pitch < 20.0f)
            camera_pitch = 20.0f;
        vec3 camera_dir = glm::angleAxis(camera_yaw, vec3(0, 0, 1)) * vec3(1, 0, 0);
        vec3 camera_right = glm::cross(camera_dir, vec3(0, 0, 1));
        vec3 camera_forward = -camera_dir;
        camera_dir = glm::angleAxis(camera_pitch, camera_right) * camera_dir;
        vec3 camera_pos = camera_focus + camera_dir * camera_dist;
        if (camera_pitch < 0.0f) {
            camera_forward = -camera_forward;
            camera_right = -camera_right;
        }
        

        mat4 perspective_matrix = glm::perspective(45.0f, aspect_ratio, 0.1f, 10000.0f);
        if (orthogonal_projection) {
            float dim = camera_dist*0.5f;
            projection_matrix = glm::ortho(-dim*aspect_ratio, dim*aspect_ratio,
                                           -dim, dim,
                                           -10000.0f, 10000.0f);
        } else {
            projection_matrix = perspective_matrix;
        }

        view_matrix = glm::lookAt(camera_pos,
                                  camera_focus,
                                  vec3(0, 0, 1));

        if (!rotating)
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

        skybox.render(view_matrix, perspective_matrix);

        simple_renderable_system.render(&renderqueue, view_matrix, projection_matrix);
        renderqueue.flush();

        if (orthogonal_projection) {
            line_vertexes.clear();
            body_system.quad_tree.gather_outlines([&](float x, float y) mutable {
                line_vertexes.push_back(LineVertex(vec3(x, y, 0), vec4(1, 1, 1, 0.1f)));
            });
            for (auto b : body_system) {
                vec3 pos = b->pos;
                pos.z = 0;
                line_vertexes.push_back(LineVertex(pos, vec4(1, 1, 1, 0.2f)));
                line_vertexes.push_back(LineVertex(b->pos, vec4(1, 1, 1, 0.2f)));
            }
            if (line_vertexes.size() > max_line_vertexes)
                line_vertexes.resize(max_line_vertexes);

            line_buf->bind();
            line_buf->write(0, sizeof(line_vertexes[0])*line_vertexes.size(), &line_vertexes[0]);
            line_buf->unbind();
            line_mesh->set_num_vertexes(line_vertexes.size());

            auto cmd = renderqueue.add_command(line_program, line_mesh);
            cmd->indexed = false;
            cmd->add_uniform("m_pvm", projection_matrix * view_matrix);

            StateContext context;
            context.depth_mask(GL_FALSE);
            context.enable(GL_BLEND);
            context.blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            renderqueue.flush();
        }
        
        SDL_GL_SwapWindow(window);


        //////////////////////////////////////////////////////////////////////////////////////////////////
        // Event handling:
        //////////////////////////////////////////////////////////////////////////////////////////////////

        float sensitivity = 0.01f;

        vec3 motion(0, 0, 0);
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] || (mx == 0 && !rotating))
            motion += camera_right;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] || (mx == mode.w - 1 && !rotating))
            motion -= camera_right;
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] || (my == 0 && !rotating))
            motion += camera_forward;
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] || (my == mode.h - 1 && !rotating))
            motion -= camera_forward;
        if (glm::length(motion) > 0) {
            motion = glm::normalize(motion);
            camera_focus += motion * sqrtf(camera_dist) * 0.2f;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_KEYDOWN:
                break;
            case SDL_KEYUP:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = false;
                if (event.key.keysym.sym == SDLK_SPACE)
                    orthogonal_projection = !orthogonal_projection;
                break;
            case SDL_MOUSEMOTION:
                if (rotating) {
                    float a = -360.f * (float)event.motion.xrel / (float)mode.w;
                    do {
                        camera_yaw += a;
                    } while (fabsf(camera_yaw) < 0.01f);

                    a = 360.f * (float)event.motion.yrel / (float)mode.h;
                    do {
                        camera_pitch += a;
                    } while (fabsf(camera_pitch) < 0.01f);

                    while (camera_yaw < 0.0f)
                        camera_yaw += 360.0f;
                    while (camera_yaw > 360.0f)
                        camera_yaw -= 360.0f;

                    if (camera_pitch > 89.0f)
                        camera_pitch = 89.0f;
                    if (camera_pitch < -89.0f)
                        camera_pitch = -89.0f;
                }
                break;
            case SDL_MOUSEWHEEL:
                camera_dist -= (camera_dist * 0.4f * event.wheel.y);
                camera_dist = clamp(camera_dist, 10.0f, 1000.0f);
                break;
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
    ship_mesh = 0;
    asteroid_mesh = 0;

    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("Done.\n");
    return 0;
}
