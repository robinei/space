#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>

#include "SDL.h"
#include "SDL_mixer.h"
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

#include "RVO3D/RVO.h"
#include "btBulletCollisionCommon.h"

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


#pragma pack(push, 1)
struct LineVertex {
    vec3 pos;
    vec4 color;
    LineVertex() {}
    LineVertex(vec3 pos, vec4 color) : pos(pos), color(color) {}
};
#pragma pack(pop)
static std::vector<LineVertex> line_vertexes;

static Entity *selected_entity = nullptr;














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
    vec3 desired_vel;
    float radius;
    Entity *entity;

    size_t rvo_agent;

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
    RVO::RVOSimulator rvo_sim;

    void update(float dt);
};








static bool sweep(Body *b0, Body *b1, float dt, float &t_out) {
    glm::vec3 v0 = b1->pos - b0->pos;
    glm::vec3 v1 = v0 + (b1->vel - b0->vel)*dt;
    float r = (b0->radius + b1->radius);

    float dot00 = glm::dot(v0, v0);
    float dot01 = glm::dot(v0, v1);
    float dot11 = glm::dot(v1, v1);

    float a = dot00 - 2.f * dot01 + dot11;
    float b = 2.f * (dot01 - dot00);
    float c = dot00 - r*r;

    float det = b*b - 4.f * a*c;

    if (det > 0.f) {
        float t = -(b + sqrtf(det)) / (2.f * a);
        if (0.f <= t && t <= 1.f) {
            t_out = t;
            return true;
        }
    }

    return false;
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

    enum { MAX_CLOSEST = 8 };
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
            //d.z = 0;
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

    vec3 obstacle_avoid() {
        float t_horizon = 5.0f;
        float best_t = 10000000.0f;
        Body *best_b = nullptr;

        vec3 sum(0, 0, 0);

        for (int i = 0; i < MAX_CLOSEST; ++i) {
            Entity *e = closest[i];
            if (!e) continue;

            Body *b = e->get_component<Body>();

            float t = 0.0f;
            if (sweep(body, b, t_horizon, t)) {
                vec3 p0 = body->pos + body->vel*t*t_horizon;
                vec3 p1 = b->pos + b->vel*t*t_horizon;
                sum += glm::normalize(p0 - p1) * (1.0f - t);

                line_vertexes.push_back(LineVertex(body->pos, vec4(0, 1, 0, 0.9f)));
                line_vertexes.push_back(LineVertex(b->pos, vec4(0, 1, 0, 0.1f)));

                line_vertexes.push_back(LineVertex(body->pos, vec4(0, 0, 1, 1)));
                line_vertexes.push_back(LineVertex(p0, vec4(0, 0, 1, 1)));

                line_vertexes.push_back(LineVertex(b->pos, vec4(1, 0, 0, 1)));
                line_vertexes.push_back(LineVertex(p1, vec4(1, 0, 0, 1)));

                if (t < best_t) {
                    best_t = t;
                    best_b = b;
                }
            }
        }

        if (!best_b)
            return vec3(0, 0, 0);

        //vec3 v = steer(glm::cross(body->vel, body->pos - best_b->pos));
        vec3 v = steer(sum);

        line_vertexes.push_back(LineVertex(body->pos, vec4(1, 1, 1, 0.8f)));
        line_vertexes.push_back(LineVertex(body->pos + v*3.0f, vec4(1, 1, 1, 0.8f)));

        return v;
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
        float brakelimit = 50.0f;
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


static RVO::Vector3 to_rvo(vec3 v) {
    return RVO::Vector3(v.x, v.y, v.z);
}

static vec3 from_rvo(RVO::Vector3 v) {
    return vec3(v.x(), v.y(), v.z());
}

static mat4 calc_rotation_matrix(vec3 dir) {
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

void Body::init(EntityManager *m, Entity *e) {
    BodySystem *sys = m->get_system<BodySystem>();
    sys->quad_tree.insert(this);
    entity = e;

    float max_vel = 0;
    Ship *s = entity->get_component<Ship>();
    if (s) {
        max_vel = s->maxspeed;
    }
    RVO::Vector3 rvo_pos = to_rvo(pos);
    rvo_agent = sys->rvo_sim.addAgent(rvo_pos, 50.0f, 16, 10.0f, radius, max_vel);
}

void BodySystem::update(float dt) {
    rvo_sim.setTimeStep(dt);
    rvo_sim.doStep();

    for (Body *b : *this) {
        rvo_sim.setAgentPrefVelocity(b->rvo_agent, to_rvo(b->desired_vel));
        b->pos = from_rvo(rvo_sim.getAgentPosition(b->rvo_agent));
        b->vel = from_rvo(rvo_sim.getAgentVelocity(b->rvo_agent));
        b->qtree_update();

        Ship *s = b->entity->get_component<Ship>();
        SimpleRenderable *r = b->entity->get_component<SimpleRenderable>();
        if (r && s) {
            r->model_matrix = glm::translate(b->pos) * calc_rotation_matrix(s->dir);
        }
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

    //acc = obstacle_avoid();

    //if (acc == vec3(0, 0, 0)) {
    acc += separation() * 1.5f;
    acc += alignment() * 1.0f;
    acc += cohesion() * 1.0f;

    acc += planehug() * 1.5f;
    //acc += zseparation() * 1.5f;

    acc += arrive(cursor_pos) * 1.5f;
    //}

    body->desired_vel += acc * dt;
    body->desired_vel = limit(body->desired_vel, maxspeed);

    vec3 v = glm::normalize(body->vel);
    float a = glm::angle(dir, v);
    if (fabsf(a) > 0.001f) {
        vec3 axis(glm::cross(dir, v));
        dir = glm::normalize(dir * glm::angleAxis(glm::min(45.0f * dt, a), axis));
    }
}







static void do_spawn_boid(EntityManager *m, vec3 pos) {
    pos.z = glm::linearRand(-10.0f, 10.0f);
    Entity *e = m->create_entity();
    
    Body *b = m->add_component<Body>(e);
    b->pos = pos;
    b->radius = ship_mesh->radius() * .5f;

    Ship *s = m->add_component<Ship>(e);
    s->dir = glm::normalize(vec3(glm::diskRand(10.0f), 0.0f));
    s->maxspeed = glm::linearRand(10.0f, 30.0f);
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
    pos.z = glm::linearRand(-10.0f, 10.0f);
    Entity *e = m->create_entity();

    Body *b = m->add_component<Body>(e);
    b->pos = pos;
    b->radius = asteroid_mesh->radius() * 10;

    SimpleRenderable *r = m->add_component<SimpleRenderable>(e);
    r->model_matrix = glm::translate(pos) * glm::scale(vec3(10, 10, 10));
    r->mesh = asteroid_mesh;
    r->program = ship_program;
    r->ambient_color = vec4(0.15f, 0.15f, 0.15f, 1);
    r->diffuse_color = vec4(0.3f, 0.3f, 0.3f, 1);
    r->specular_color = vec4(0.4f, 0.4f, 0.4f, 1);
    r->shininess = 56.8f;

    m->optimize_entity(e);
    m->init_entity(e);
}






static Entity *closest_to_mouse(EntityManager *manager) {
    BodySystem *sys = manager->get_system<BodySystem>();

    Body *best = nullptr;
    float best_dist = 100000.0f;

    sys->quad_tree.query(cursor_pos.x - 50, cursor_pos.y - 50,
                         cursor_pos.x + 50, cursor_pos.y + 50,
                         [&](QuadTree::Object *obj) mutable
    {
        Body *b = static_cast<Body *>(obj);
        if (!best) {
            best = b;
        } else {
            vec3 d = b->pos - cursor_pos;
            float dist = glm::length(d);
            if (dist < best_dist) {
                best = b;
                best_dist = dist;
            }
        }
    });

    if (!best)
        return nullptr;
    return best->entity;
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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
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


    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
        die("Mix_OpenAudio() error: %s", Mix_GetError());
    }
    Mix_Music *music = Mix_LoadMUS("../data/music/test.ogg");
    if (!music) {
        printf("Mix_LoadMUS() error: %s\n", Mix_GetError());
    } else {
        Mix_PlayMusic(music, -1);
    }


    StateContext context; // create a root context
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
        do_spawn_boid(&entity_manager, vec3(glm::diskRand(100.0f), 0.0f));
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

    /*vec3 up(0, 0, 1);
    vec3 forward(glm::normalize(dir));
    vec3 right(glm::cross(forward, up));
    up = glm::cross(right, forward);
    right = glm::normalize(glm::cross(forward, up));
    up = glm::normalize(glm::cross(right, forward));*/
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
        vec3 camera_forward = -camera_dir;
        vec3 camera_right = glm::cross(camera_dir, vec3(0, 0, 1));
        camera_dir = glm::angleAxis(camera_pitch, camera_right) * camera_dir;
        vec3 camera_up = glm::cross(camera_right, camera_dir);
        vec3 camera_pos = camera_focus + camera_dir * camera_dist;
        

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
        
        if (selected_entity) {
            if (selected_entity->dying()) {
                selected_entity = nullptr;
            }  else {
                Body *b = selected_entity->get_component<Body>();
                camera_focus = b->pos;
            }
        }

        Entity *hovered_entity = closest_to_mouse(&entity_manager);

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

        {
            if (orthogonal_projection) {
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
            }

            if (hovered_entity) {
                Body *b = hovered_entity->get_component<Body>();

                vec3 p0 = b->pos - camera_right*b->radius;
                vec3 p1 = b->pos - camera_up*b->radius;
                vec3 p2 = b->pos + camera_right*b->radius;
                vec3 p3 = b->pos + camera_up*b->radius;

                vec4 c(1, 1, 1, 0.5f);

                line_vertexes.push_back(LineVertex(p0, c));
                line_vertexes.push_back(LineVertex(p1, c));

                line_vertexes.push_back(LineVertex(p1, c));
                line_vertexes.push_back(LineVertex(p2, c));

                line_vertexes.push_back(LineVertex(p2, c));
                line_vertexes.push_back(LineVertex(p3, c));

                line_vertexes.push_back(LineVertex(p3, c));
                line_vertexes.push_back(LineVertex(p0, c));
            }

            line_buf->bind();
            line_buf->write(0, sizeof(line_vertexes[0])*line_vertexes.size(), &line_vertexes[0]);
            line_buf->unbind();
            line_mesh->set_num_vertexes(line_vertexes.size());
            line_vertexes.clear();

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
        if (glm::length(motion) > 0 && !selected_entity) {
            motion = glm::normalize(motion);
            camera_focus += motion * sqrtf(camera_dist) * 0.2f;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_f)
                    add_asteroid(&entity_manager, cursor_pos);
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
                } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                    selected_entity = hovered_entity;
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

    if (music) {
        printf("Freeing music...\n");
        Mix_FreeMusic(music);
    }
    printf("Closing audio...\n");
    Mix_CloseAudio();
    printf("Deleting OpenGL context...\n");
    SDL_GL_DeleteContext(glcontext);
    printf("Destroying window...\n");
    SDL_DestroyWindow(window);
    printf("Shutting down SDL...\n");
    SDL_Quit();
    printf("Done.\n");
    return 0;
}
