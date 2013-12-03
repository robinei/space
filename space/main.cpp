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
#include "fpscamera.h"
#include "quadtree.h"
#include "renderqueue.h"
#include "mtrand.h"

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

static mat4 projection_matrix;
static mat4 view_matrix;
static vec3 light_dir;
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
    ship_mesh = load_mesh("../data/meshes/harv.ply");

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










template <int BucketsBits, typename T, class KeyFunc=T>
struct FixedHashTable {
    enum {
        HighBits = BucketsBits,
        LowBits = 32 - HighBits,
        NumBuckets = 1 << HighBits
    };

    typedef unsigned int KeyType;


    // random constant used in the universal hash function
    // (http://en.wikipedia.org/wiki/Universal_hashing)
    // we randomly generate this in order to make new hash functions until
    // one is found which results in low max_probe (hopefully 0)
    unsigned int hash_a : LowBits;

    // the longest probe needed to reach any value stored in this block
    unsigned int max_probe : HighBits;

    T buckets[NumBuckets];


    FixedHashTable() :
        hash_a(1870964089), // chosen by a fair dice roll (must be positive and odd)
        max_probe(0)
    {
        for (int i = 0; i < NumBuckets; ++i)
            buckets[i] = T();
    }

    bool insert(T val) {
        unsigned int h = (hash_a * KeyFunc::key(val)) >> LowBits;

        // probe for a free slot
        unsigned int p = 0;
        do {
            unsigned int j = (h + p) & (NumBuckets - 1);
            if (buckets[j] == T()) {
                buckets[j] = val;
                if (p > max_probe)
                    max_probe = p; // this probe was the longest yet
                return true;
            }
        } while (++p < NumBuckets);

        return false;
    }
    
    T lookup(KeyType key) {
        unsigned int h = (hash_a * key) >> LowBits;

        // probe from the hashed slot
        unsigned int p = 0;
        do {
            unsigned int j = (h + p) & (NumBuckets - 1);
            T val(buckets[j]);
            if (val != T() && KeyFunc::key(val) == key)
                return val;
        } while (++p <= max_probe);

        return T();
    }

    // rehash using randomly generated hash_a until satisfied with max_probe
    template <class RandFunc>
    void optimize(RandFunc &rnd, int max_attempts=1000) {
        T result[NumBuckets];

        if (max_probe == 0)
            return; // already optimal

        // the current max_probe is the one to beat
        unsigned int best_a = 0;
        unsigned int best_max_p = max_probe;

        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            for (int i = 0; i < NumBuckets; ++i)
                result[i] = T();

            // generate new random hash_a (which must be positive and odd)
            unsigned int a = ((rnd() & ((1 << LowBits) - 1)) & ~(unsigned int)1) + 1;
            unsigned int max_p = 0;

            // try to insert the values using the new hash function while
            // keeping record of the longest probe length encountered (max_p)
            for (int i = 0; i < NumBuckets; ++i) {
                T val = buckets[i];
                if (val == T())
                    continue;

                unsigned int h = (a * KeyFunc::key(val)) >> LowBits;

                // probe until we find a free slot
                unsigned int p = 0;
                do {
                    unsigned int j = (h + p) & (NumBuckets - 1);
                    if (result[j] == T()) {
                        result[j] = val;
                        break;
                    }
                } while (++p < NumBuckets);

                if (p > max_p)
                    max_p = p;
            }

            if (max_p < best_max_p) {
                best_a = a;
                best_max_p = max_p;
                if (max_p == 0)
                    break; // perfect; no point looking any more
            }
        }

        if (best_max_p < max_probe) {
            hash_a = best_a;
            max_probe = best_max_p;
            for (int i = 0; i < NumBuckets; ++i)
                buckets[i] = result[i];
        }
    }
};







typedef unsigned int ComponentType;

class Component {
public:
    virtual ~Component() {}
    virtual ComponentType type() = 0;
};


// we store component refs in an associative container that has the structure
// of a linked list of fixed size hash tables using open addressing scheme.
// we use universal hashing to ensure that most components fall into the exact
// bucket that their type hashes to, meaning that probe lengths are usually 0.
// chained blocks are independent, so lookup operations must try all blocks
// until they find the type they are looking for, or fail.
// HighBits should be set so that only one or two blocks are needed per entity.
struct ComponentBlock {
    enum {
        HighBits = 3, // set to 4 for 16 buckets etc.
        LowBits = 32 - HighBits,
        NumBuckets = 1 << HighBits
    };

    // random constant used in the universal hash function
    // (http://en.wikipedia.org/wiki/Universal_hashing)
    // we randomly generate this in order to make new hash functions until
    // one is found which results in low max_probe (hopefully 0)
    unsigned int hash_a : LowBits;

    // the longest probe needed to reach any value stored in this block
    unsigned int max_probe : HighBits;

    Component *buckets[NumBuckets];

    // if the buckets overflow, we allocate a new block.
    ComponentBlock *next;


    ComponentBlock();

    // rehash using randomly generated hash_a until satisfied with max_probe
    void optimize();
};


// an entry alway starts with one embedded component block
struct Entity {
    ComponentBlock block;

    void insert(Component *c);
    Component *lookup(ComponentType type);
    void optimize();
};



static inline unsigned int gen_hash_a() {
    return ((rand() & ((1 << ComponentBlock::LowBits) - 1)) & ~(unsigned int)1) + 1;
}

ComponentBlock::ComponentBlock() : max_probe(0), next(nullptr)
{
    hash_a = gen_hash_a();
    memset(buckets, 0, sizeof(buckets));
}

void ComponentBlock::optimize() {
    Component *result[NumBuckets];

    if (max_probe == 0)
        return; // already optimal

    // the current max_probe is the one to beat
    unsigned int best_a = 0;
    unsigned int best_max_p = max_probe;

    for (int attempt = 0; attempt < 1000; ++attempt) {
        memset(result, 0, sizeof(result));

        // generate new random hash_a (which must be positive and odd)
        unsigned int a = gen_hash_a();
        unsigned int max_p = 0;

        // try to insert the values using the new hash function while
        // keeping record of the longest probe length encountered (max_p)
        for (int i = 0; i < NumBuckets; ++i) {
            Component *c = buckets[i];
            if (!c)
                continue;
            
            unsigned int h = (a * c->type()) >> LowBits;

            // probe until we find a free slot
            unsigned int p = 0;
            do {
                unsigned int j = (h + p) & (NumBuckets - 1);
                if (!result[j]) {
                    result[j] = c;
                    break;
                }
            } while (++p < NumBuckets);

            if (p > max_p)
                max_p = p;
        }

        if (max_p < best_max_p) {
            best_a = a;
            best_max_p = max_p;
            if (max_p == 0)
                break; // perfect; no point looking any more
        }
    }

    if (best_max_p < max_probe) {
        hash_a = best_a;
        max_probe = best_max_p;
        memcpy(buckets, result, sizeof(result));
    }
}


void Entity::insert(Component *c) {
    ComponentBlock *b = &block;
    ComponentType type = c->type();
    
    do {
        unsigned int h = (b->hash_a * type) >> ComponentBlock::LowBits;

        // probe for a free slot
        unsigned int p = 0;
        do {
            unsigned int j = (h + p) & (ComponentBlock::NumBuckets - 1);
            if (!b->buckets[j]) {
                b->buckets[j] = c;
                if (p > b->max_probe)
                    b->max_probe = p; // this probe was the longest yet
                return;
            }
        } while (++p < ComponentBlock::NumBuckets);

        // no free slots; try the next block
        b = b->next;
    } while (b);
    
    assert(0);
    // TODO: add new block
}

Component *Entity::lookup(ComponentType type) {
    ComponentBlock *b = &block;
    
    do {
        unsigned int h = (b->hash_a * type) >> ComponentBlock::LowBits;

        // probe from the hashed slot
        unsigned int p = 0;
        do {
            unsigned int j = (h + p) & (ComponentBlock::NumBuckets - 1);
            Component *c = b->buckets[j];
            if (c && c->type() == type)
                return c;
        } while (++p <= b->max_probe);

        // no match in this block, so we check the next (if any)
        b = b->next;
    } while (b);
    
    // it wasn't found in any of the blocks
    return nullptr;
}

void Entity::optimize() {
    ComponentBlock *b = &block;
    do {
        b->optimize();
        b = b->next;
    } while (b);
}





static const char *fourcc_str(unsigned int fourcc) {
    static char str[5];
    str[0] = (fourcc >> 24) & 0xff; if (str[0] == 0) str[0] = ' ';
    str[1] = (fourcc >> 16) & 0xff; if (str[1] == 0) str[1] = ' ';
    str[2] = (fourcc >> 8) & 0xff; if (str[2] == 0) str[2] = ' ';
    str[3] = fourcc & 0xff; if (str[3] == 0) str[3] = ' ';
    str[4] = 0;
    return str;
}

struct Chameleon : public Component {
    ComponentType t;

    Chameleon(ComponentType t) : t(t) {}

    ComponentType type() override { return t; }
};


struct IntKey {
    static unsigned int key(int x) {
        return (unsigned long)x;
    }
};

typedef FixedHashTable<10, int, IntKey> IntTable;

typedef void *Ptr;
static int rand_count = 0;
static MTRand_int32 mtrand;

struct Rand {
    unsigned long operator()() {
        ++rand_count;
        return mtrand();
    }
};

static void teste() {
    assert(0 == int());
    assert(1 != int());
    assert(nullptr == Ptr());

    IntTable table;

    /*assert(table.insert(10));
    assert(table.insert(3));
    assert(table.insert(60));
    assert(table.insert(85));
    assert(table.insert(49));
    assert(table.insert(36));
    assert(table.insert(305));
    table.optimize(Rand());
    printf("rand_count: %d\n", rand_count);
    printf("max_probe: %d\n", table.max_probe);
    printf("%d\n", table.lookup(10));
    printf("%d\n", table.lookup(3));
    printf("%d\n", table.lookup(11));*/

    for (int i = 0; i < 512; ++i)
        assert(table.insert(mtrand()));

    printf("max_probe0: %d\n", table.max_probe);
    table.optimize(Rand());
    printf("rand_count: %d\n", rand_count);
    printf("max_probe: %d\n", table.max_probe);

    return;
    for (int i = 0; i < 1000; ++i)
        rand();




    Chameleon pos('POS');
    Chameleon vel('VEL');
    Chameleon ship('SHIP');
    Chameleon anim('ANIM');
    Chameleon boid('BOID');
    Chameleon body('BODY');
    Chameleon gfx('GFX');
    Chameleon snd('SND');

    Entity e;

    e.insert(&pos);
    e.insert(&vel);
    e.insert(&ship);
    e.insert(&anim);
    e.insert(&boid);
    e.insert(&body);
    e.insert(&gfx);
    e.insert(&snd);

    e.optimize();

    printf("max_probe: %d\n", e.block.max_probe);
    for (int i = 0; i < ComponentBlock::NumBuckets; ++i) {
        Component *c = e.block.buckets[i];
        if (!c)
            printf("name: null\n");
        else
            printf("name: %s\n", fourcc_str(c->type()));
    }

    assert(e.lookup('POS') && e.lookup('POS')->type() == 'POS');
    assert(e.lookup('VEL')->type() == 'VEL');
    assert(e.lookup('SHIP')->type() == 'SHIP');
    assert(e.lookup('ANIM')->type() == 'ANIM');
    assert(!e.lookup('NOPE'));
}






















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
    virtual void render(RenderQueue *renderer) {}

private:
    Body(const Body &);
    Body &operator=(const Body &);
};


void free_body(Body *b);

class World {
public:
    typedef List<Body, &Body::world_link> BodyList;

    BodyList bodies;
    QuadTree quadtree;
    float dt;

    World() : quadtree(Rect(-1000, -1000, 1000, 1000), 7) {}

    ~World() {
        while (!bodies.empty())
            free_body(bodies.front());
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
        for (Body *body : bodies)
            body->render(&renderqueue);
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

vec3 limit(vec3 v, float len) {
    if (glm::length(v) > len)
        return glm::normalize(v) * len;
    return v;
}

class Boid : public Body {
public:
    vec3 dir;
    float maxspeed;
    float maxforce;
    int team;

    void update(World &world) {
        UpdateContext context(world);

        context.quadtree.query(vec2(pos), 50.0f, context.neighbours);
        //printf("context.neighbours: %d\n", context.neighbours.size());

        vec3 acc(0, 0, 0);

        acc += separation(context) * 1.5f;
        acc += alignment(context) * 1.0f;
        acc += cohesion(context) * 1.0f;

        acc += planehug(context) * 1.5f;
        acc += zseparation(context) * 1.5f;

        acc += seek(cursor_pos) * 1.0f;

        vel += acc * world.dt;
        vel = limit(vel, maxspeed);

        pos += vel * world.dt;
        //pos.z = 0;

        qtree_update();

        vec3 v = glm::normalize(vel);
        float a = glm::angle(dir, v);
        if (fabsf(a) > 0.001f) {
            vec3 axis(glm::cross(dir, v));
            dir = glm::normalize(dir * glm::angleAxis(glm::min(45.0f * world.dt, a), axis));
        }
    }

    vec3 planehug(UpdateContext &context) {
        vec3 target = pos;
        target.z = 0;
        return arrive(target);
    }

    vec3 zseparation(UpdateContext &context) {
        float sep = 20.0f;
        vec3 sum(0, 0, 0);
        int count = 0;
        for (auto obj : context.neighbours) {
            Boid *b = static_cast<Boid *>(obj);
            if (b == this) continue;
            vec3 d = pos - b->pos;
            float len = glm::length(d);
            if (len > sep || len <= 0.00001f) continue;
            //d = glm::normalize(d);
            float dz = d.z;
            if (dz == 0.0f)
                dz = glm::dot(glm::normalize(vel), glm::normalize(b->vel));
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

    vec3 separation(UpdateContext &context) {
        float sep = 20.0f;
        vec3 sum(0, 0, 0);
        int count = 0;
        for (auto obj : context.neighbours) {
            Boid *b = static_cast<Boid *>(obj);
            if (b == this) continue;
            vec3 d = pos - b->pos;
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

    vec3 alignment(UpdateContext &context) {
        float neighbordist = 50;
        vec3 sum(0, 0, 0);
        int count = 0;
        for (auto obj : context.neighbours) {
            Boid *b = static_cast<Boid *>(obj);
            if (b == this) continue;
            if (b->team != team) continue;
            vec3 d = pos - b->pos;
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

    vec3 cohesion(UpdateContext &context) {
        float neighbordist = 50;
        vec3 sum(0, 0, 0);
        int count = 0;
        for (auto obj : context.neighbours) {
            Boid *b = static_cast<Boid *>(obj);
            if (b == this) continue;
            if (b->team != team) continue;
            vec3 d = pos - b->pos;
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
        return steer(target - pos);
    }

    vec3 steer(vec3 dir) {
        float len = glm::length(dir);
        if (len < 0.000001f)
            return vec3(0, 0, 0);
        dir *= maxspeed / len;
        return limit(dir - vel, maxforce);
    }

    vec3 arrive(vec3 target) {
        float brakelimit = 10.0f;
        vec3 desired = target - pos;
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

    void render(RenderQueue *renderqueue) {
        mat4 model = glm::translate(pos) * calc_rotation_matrix();
        mat4 vm = view_matrix * model;
        mat4 pvm = projection_matrix * vm;
        mat3 normal = glm::inverseTranspose(mat3(vm));

        auto cmd = renderqueue->add_command(ship_program, ship_mesh);
        cmd->add_uniform("m_pvm", pvm);
        cmd->add_uniform("m_vm", vm);
        cmd->add_uniform("m_normal", normal);
        cmd->add_uniform("light_dir", light_dir);
        if (team == 0) {
            cmd->add_uniform("mat_ambient", vec4(0.25f, 0.25f, 0.25f, 1));
            cmd->add_uniform("mat_diffuse", vec4(0.4f, 0.4f, 0.4f, 1));
            cmd->add_uniform("mat_specular", vec4(0.774597f, 0.774597f, 0.774597f, 1));
            cmd->add_uniform("mat_shininess", 76.8f);
        } else {
            cmd->add_uniform("mat_ambient", vec4(0.329412f, 0.223529f, 0.027451f, 1.0f));
            cmd->add_uniform("mat_diffuse", vec4(0.780392f, 0.568627f, 0.113725f, 1.0f));
            cmd->add_uniform("mat_specular", vec4(0.992157f, 0.941176f, 0.807843f, 1.0f));
            cmd->add_uniform("mat_shininess", 27.89743616f);
        }
    }
};


Pool<Boid> boid_pool;

static void spawn_boid(World &world, vec3 pos) {
    Boid *b = boid_pool.create();
    b->pos = pos;
    b->vel = vec3(glm::diskRand(10.0f), 0.0f);
    b->dir = glm::normalize(b->vel);
    b->maxspeed = 40;
    b->maxforce = 1;
    b->team = rand() % 2;
    world.add_body(b);
}

void free_body(Body *b) {
    boid_pool.free(static_cast<Boid *>(b));
}

























int main(int argc, char *argv[]) {
#ifdef WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CON", "w", stdout);
        freopen("CON", "w", stderr);
    }
#endif
    teste();
    return 0;
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

    SDL_DisplayMode mode;
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

    glEnable(GL_MULTISAMPLE);

    try {
        LoadTriangle();
    } catch (const std::exception &e) {
        die("error: %s", e.what());
    }


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

    World world;

    for (int i = 0; i < 40; ++i) {
        spawn_boid(world, vec3(glm::diskRand(200.0f), 0.0f));
    }

    vec3 camera_pos(0, -5, 20);
    vec3 camera_right(1, -1, 0);
    vec3 camera_forward(1, 1, 0);
    float aspect_ratio = (float)mode.w / (float)mode.h;
    bool orthogonal_projection = true;

    light_dir = glm::normalize(vec3(1, 0, 3));

    const Uint8 *keys = SDL_GetKeyboardState(0);
    Uint32 prevticks = SDL_GetTicks();
    bool running = true;

    while (running) {
        //////////////////////////////////////////////////////////////////////////////////////////////////
        // Updating:
        //////////////////////////////////////////////////////////////////////////////////////////////////

        Uint32 ticks = SDL_GetTicks();
        Uint32 tickdiff = ticks - prevticks;
        prevticks = ticks;
        float dt = (float)tickdiff / 1000.0f;
        world.dt = dt;

        int mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);

        if (orthogonal_projection) {
            float dim = camera_pos.z;
            projection_matrix = glm::ortho(-dim*aspect_ratio, dim*aspect_ratio,
                                           -dim, dim,
                                           -10000.0f, 10000.0f);
        } else {
            projection_matrix = glm::perspective(45.0f, aspect_ratio, 0.1f, 10000.0f);
        }

        view_matrix = glm::lookAt(camera_pos,
                                  camera_pos + vec3(1, 1, -1),
                                  vec3(0, 0, 1));

        //light_dir = glm::normalize(glm::angleAxis(dt*10.0f, vec3(0, 0, 1)) * light_dir);

        {
            vec3 p0 = glm::unProject(vec3(mx, mode.h - my - 1, 0), view_matrix, projection_matrix, vec4(0, 0, mode.w, mode.h));
            vec3 p1 = glm::unProject(vec3(mx, mode.h - my - 1, 1), view_matrix, projection_matrix, vec4(0, 0, mode.w, mode.h));
            cursor_pos = Plane::XY().ray_intersect(p0, p1);
        }

        world.update();
        //world.bodies.front()->pos = cursor_pos;


        //////////////////////////////////////////////////////////////////////////////////////////////////
        // Rendering:
        //////////////////////////////////////////////////////////////////////////////////////////////////

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        {
            qtree_lines.clear();
            world.quadtree.gather_outlines(qtree_lines);
            if (qtree_lines.size() > qtree_max_vertexes)
                qtree_lines.resize(qtree_max_vertexes);
            qtree_buf->bind();
            qtree_buf->write(0, sizeof(qtree_lines[0])*qtree_lines.size(), &qtree_lines[0]);
            qtree_buf->unbind();
            qtree_mesh->set_num_vertexes(qtree_lines.size());

            auto cmd = renderqueue.add_command(qtree_program, qtree_mesh);
            cmd->add_uniform("m_pvm", projection_matrix * view_matrix);
            cmd->add_uniform("color", vec4(0.2f, 0.2f, 0.2f, 1));
            glDepthMask(GL_FALSE);
            renderqueue.flush();
            glDepthMask(GL_TRUE);
        }

        {
            vlines_lines.clear();
            for (auto b : world.bodies) {
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
            cmd->add_uniform("m_pvm", projection_matrix * view_matrix);
            cmd->add_uniform("color", vec4(0.5f, 0.5f, 0.5f, 1));
            renderqueue.flush();
        }

        world.render();
        renderqueue.flush();
        
        SDL_GL_SwapWindow(window);


        //////////////////////////////////////////////////////////////////////////////////////////////////
        // Event handling:
        //////////////////////////////////////////////////////////////////////////////////////////////////

        float speed = 1.0;
        float sensitivity = 0.01f;

        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] || mx == 0)
            camera_pos -= camera_right*camera_pos.z*dt*speed;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] || mx == mode.w - 1)
            camera_pos += camera_right*camera_pos.z*dt*speed;
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] || my == 0)
            camera_pos += camera_forward*camera_pos.z*dt*speed;
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] || my == mode.h - 1)
            camera_pos -= camera_forward*camera_pos.z*dt*speed;

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
                break;
            case SDL_MOUSEWHEEL: {
                vec3 d = (cursor_pos - camera_pos) * (0.4f * event.wheel.y);
                vec3 p = camera_pos + d;
                float min_z = 10.0f;
                float max_z = 1000.0f;
                if (p.z < min_z)
                    p = Plane::XY(-min_z).ray_intersect(camera_pos, cursor_pos);
                else if (p.z > max_z)
                    p = Plane::XY(-max_z).ray_intersect(cursor_pos, camera_pos);
                camera_pos = p;
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
                spawn_boid(world, cursor_pos);
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
