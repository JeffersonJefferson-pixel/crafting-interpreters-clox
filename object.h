#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

// check if value is function object.
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)

// convert object value to function object.
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

typedef struct {
    Obj obj;
    int arity; // number of parameters.
    Chunk chunk; // each function has its own chunk.
    ObjString* name; // function name.
} ObjFunction;

// native function takes argument count and pointer to first argument on the stack.
typedef Value (*NativeFn)(int argCount, Value* args);

// it doesn't push a callframe when called. it has no bytecode.
typedef struct {
    Obj obj; // Obj header
    NativeFn function; // pointer to C function.
} ObjNative;

struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

// create new function.
ObjFunction* newFunction();
// create native function.
ObjNative* newNative(NativeFn function);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif