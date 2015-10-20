#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

typedef enum {
    PONE_UNDEF,
    PONE_INT,
    PONE_NUM,
    PONE_STRING
} pone_t;

#define PONE_HEAD \
    int refcnt; \
    pone_t type;

typedef struct {
    PONE_HEAD;
} pone_val;

typedef struct {
    PONE_HEAD;
} pone_undef;

// integer value
typedef struct {
    PONE_HEAD;
    int i;
} pone_int;

typedef struct {
    PONE_HEAD;
    int i;
} pone_number;

typedef struct {
    PONE_HEAD;
    const char* p;
    size_t len;
} pone_string;

typedef struct {
    size_t* savestack;
    size_t savestack_idx;
    size_t savestack_max;

    // mortals we've made
    pone_val** tmpstack;
    size_t tmpstack_idx;
    size_t tmpstack_floor;
    size_t tmpstack_max;
} pone_world;

static pone_val pone_undef_val = { -1, PONE_UNDEF };

// SV ops
size_t pone_int_val(pone_val* val);
const char* pone_string_ptr(pone_val* val);
size_t pone_string_len(pone_val* val);
void pone_refcnt_dec(pone_world* world, pone_val* val);

// scope
pone_val* pone_mortalize(pone_world* world, pone_val* val);

pone_val* pone_new_int(pone_world* world, int i);
pone_val* pone_new_str(pone_world* world, const char*p, size_t len);
pone_val* pone_str(pone_world* world, pone_val* val);
pone_t pone_type(pone_val* val);
void* pone_malloc(pone_world* world, size_t size);
void pone_die(pone_world* world, const char* str);

pone_world* pone_new_world() {
    // we can't use pone_malloc yet.
    pone_world* world = malloc(sizeof(pone_world));
    if (!world) {
        fprintf(stderr, "Cannot make world\n");
    }
    memset(world, 0, sizeof(pone_world));

    world->savestack = malloc(sizeof(size_t*) * 64);
    world->savestack_max = 64;

    world->tmpstack = malloc(sizeof(size_t*) * 64);
    world->tmpstack_max = 64;

    return world;
}

void pone_destroy_world(pone_world* world) {
    free(world->savestack);
    free(world->tmpstack);
    free(world);
}

void pone_dd(pone_world* world, pone_val* val) {
    switch (pone_type(val)) {
        case PONE_STRING:
            printf("(string: ");
            fwrite(pone_string_ptr(val), 1, pone_string_len(val), stdout);
            printf(")\n");
            break;
        case PONE_INT:
            printf("(int: %d)\n", pone_int_val(val));
            break;
        case PONE_UNDEF:
            printf("(undef)\n");
            break;
        default:
            abort();
    }
}

pone_val*  pone_builtin_dd(pone_world* world, pone_val* val) {
    pone_dd(world, val);
    return &pone_undef_val;
}

pone_val*  pone_builtin_abs(pone_world* world, pone_val* val) {
    switch (pone_type(val)) {
    case PONE_INT: {
        int i = pone_int_val(val);
        if (i < 0) {
            return pone_mortalize(world, pone_new_int(world, -i));
        } else {
            return val;
        }
    }
                   // TODO: NV
    }

    pone_die(world, "you can't call abs() for non-numeric value");
}

inline pone_t pone_type(pone_val* val) {
    return val->type;
}

// decrement reference count
inline void pone_refcnt_dec(pone_world* world, pone_val* val) {
    assert(val != NULL);

    val->refcnt--;
    if (val->refcnt == 0) {
        switch (pone_type(val)) {
        case PONE_STRING:
            free((char*)((pone_string*)val)->p);
            break;
        }
        free(val);
    }
}

inline const char* pone_string_ptr(pone_val* val) {
    assert(pone_type(val) == PONE_STRING);
    return ((pone_string*)val)->p;
}

inline size_t pone_string_len(pone_val* val) {
    assert(pone_type(val) == PONE_STRING);
    return ((pone_string*)val)->len;
}

inline size_t pone_int_val(pone_val* val) {
    assert(pone_type(val) == PONE_INT);
    return ((pone_int*)val)->i;
}

void pone_die(pone_world* world, const char* str) {
    fprintf(stderr, "%s\n", str);
    exit(1);
}

int pone_to_int(pone_world* world, pone_val* val) {
    switch (pone_type(val)) {
    case PONE_UNDEF:
        pone_die(world, "Use of uninitialized value as integer");
        break;
    case PONE_INT:
        return pone_int_val(val);
    case PONE_STRING: {
        char *end = (char*)pone_string_ptr(val) + pone_string_len(val);
        return strtol(pone_string_ptr(val), &end, 10);
    }
    default:
        abort();
    }
}

// TODO: support NV
pone_val* pone_add(pone_world* world, pone_val* v1, pone_val* v2) {
    int i1 = pone_to_int(world, v1);
    int i2 = pone_to_int(world, v2);
    return pone_mortalize(world, pone_new_int(world, i1 + i2));
}

// TODO: support NV
pone_val* pone_subtract(pone_world* world, pone_val* v1, pone_val* v2) {
    int i1 = pone_to_int(world, v1);
    int i2 = pone_to_int(world, v2);
    return pone_mortalize(world, pone_new_int(world, i1 - i2));
}

pone_val* pone_multiply(pone_world* world, pone_val* v1, pone_val* v2) {
    int i1 = pone_to_int(world, v1);
    int i2 = pone_to_int(world, v2);
    return pone_mortalize(world, pone_new_int(world, i1 * i2));
}

pone_val* pone_divide(pone_world* world, pone_val* v1, pone_val* v2) {
    int i1 = pone_to_int(world, v1);
    int i2 = pone_to_int(world, v2);
    return pone_mortalize(world, pone_new_int(world, i1 / i2)); // TODO: We should upgrade value to NV
}

pone_val* pone_str_from_int(pone_world* world, int i) {
    // INT_MAX=2147483647. "2147483647".elems = 10
    char buf[11+1];
    int size = snprintf(buf, 11+1, "%d", i);
    return pone_mortalize(world, pone_new_str(world, buf, size));
}

pone_val* pone_str(pone_world* world, pone_val* val) {
    switch (pone_type(val)) {
    case PONE_UNDEF:
        return pone_new_str(world, "(undef)", strlen("(undef)"));
    case PONE_INT:
        return pone_str_from_int(world, pone_int_val(val));
    case PONE_STRING:
        return val;
    default:
        abort();
    }
}

pone_val* pone_builtin_print(pone_world* world, pone_val* val) {
    pone_val* str = pone_str(world, val);
    fwrite(pone_string_ptr(str), sizeof(char), pone_string_len(str), stdout);
    return &pone_undef_val;
}

pone_val* pone_builtin_say(pone_world* world, pone_val* val) {
    pone_builtin_print(world, val);
    fwrite("\n", sizeof(char), 1, stdout);
    return &pone_undef_val;
}

// TODO: implement memory pool
void* pone_malloc(pone_world* world, size_t size) {
    void* p = malloc(size);
    if (!p) {
        fprintf(stderr, "Cannot allocate memory\n");
        exit(1);
    }
    memset(p, 0, size);
    return p;
}

const char* pone_strdup(pone_world* world, const char* src, size_t size) {
    void* p = malloc(size);
    if (!p) {
        fprintf(stderr, "Cannot allocate memory\n");
        exit(1);
    }
    memcpy(p, src, size);
    return p;
}

pone_val* pone_mortalize(pone_world* world, pone_val* val) {
    world->tmpstack[world->tmpstack_idx] = val;
    ++world->tmpstack_idx;
    if (world->tmpstack_idx > world->tmpstack_max) {
        world->tmpstack_max *= 2;
        pone_val** ssp = realloc(world->tmpstack, sizeof(pone_val*)*world->tmpstack_max);
        if (!ssp) {
            pone_die(world, "Cannot allocate memory");
        }
        world->tmpstack = ssp;
    }
    return val;
}

pone_val* pone_new_int(pone_world* world, int i) {
    pone_int* iv = (pone_int*)pone_malloc(world, sizeof(pone_int));
    iv->refcnt = 1;
    iv->type   = PONE_INT;
    iv->i = i;
    return (pone_val*)iv;
}

pone_val* pone_new_str(pone_world* world, const char*p, size_t len) {
    pone_string* pv = (pone_string*)pone_malloc(world, sizeof(pone_string));
    pv->refcnt = 1;
    pv->type = PONE_STRING;
    pv->p = pone_strdup(world, p, len);
    pv->len = len;
    return (pone_val*)pv;
}

void pone_enter(pone_world* world) {
    // save original tmpstack_floor
    world->savestack[world->savestack_idx] = world->tmpstack_floor;
    ++world->savestack_idx;
    if (world->savestack_max+1 < world->savestack_idx) {
        // grow it
        world->savestack_max *= 2;
        size_t* ssp = realloc(world->savestack, sizeof(size_t)*world->savestack_max);
        if (!ssp) {
            pone_die(world, "Cannot allocate memory");
        }
        world->savestack = ssp;
    }

    // save current tmpstack_idx
    world->tmpstack_floor = world->tmpstack_idx;
}

void pone_leave(pone_world* world) {
    // decrement refcnt for mortalized values
    while (world->tmpstack_idx > world->tmpstack_floor) {
        pone_refcnt_dec(world, world->tmpstack[world->tmpstack_idx-1]);
        --world->tmpstack_idx;
    }

    // pop tmpstack_floor
    --world->savestack_idx;
}

#ifdef PONE_TESTING

int main(int argc, char** argv) {
    pone_world* world = pone_new_world();

    pone_enter(world);

    pone_val* iv = pone_mortalize(world, pone_new_int(world, 4963));
    pone_builtin_say(world, iv);

    {
        pone_val* iv1 = pone_mortalize(world, pone_new_int(world, 4963));
        pone_val* iv2 = pone_mortalize(world, pone_new_int(world, 5963));
        pone_val* result = pone_add(world, iv1, iv2);
        pone_builtin_say(world, result);
    }

    {
        pone_val* iv1 = pone_mortalize(world, pone_new_int(world, 4649));
        pone_val* iv2 = pone_mortalize(world, pone_new_int(world, 5963));
        pone_val* result = pone_subtract(world, iv1, iv2);
        pone_builtin_say(world, result);
    }

    pone_val* pv = pone_mortalize(world, pone_new_str(world, "Hello, world!", strlen("Hello, world!")));
    pone_builtin_say(world, pv);

    pone_leave(world);

    pone_destroy_world(world);
}

#endif
