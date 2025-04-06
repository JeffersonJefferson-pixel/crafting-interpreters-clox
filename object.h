#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

// check if value is class object.
#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
// check if value is closure object.
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
// check if value is function object.
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
// check if value is instance object.
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)

// convert value to class object.
#define AS_CLASS(value) ((ObjClass*)AS_OBJ(value))
// convert object value to closure object.
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
// convert object value to function object.
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
// covert value to instance object.
#define AS_INSTANCE(value) ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE
} ObjType;

struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj* next;
};

typedef struct {
    Obj obj;
    int arity; // number of parameters.
    int upvalueCount; // number of upvalue.
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

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed; // closed upvalue on heap.
    struct ObjUpvalue* next; // pointer to the next upvalue in the linked list.
} ObjUpvalue;

struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

// ObjClosure wrap ObjFunction and capture surrounding local variables.
typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues; // pointer to array of upvalue pointers.
    int upvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    ObjString* name;
} ObjClass;

typedef struct {
    Obj obj;
    ObjClass* klass; // pointer to class thaat it is an instance of.
    Table fields; // state of instance by hash table.
} ObjInstance;

// create new class.
ObjClass* newClass(ObjString* name);
// create new closure.
ObjClosure* newClosure(ObjFunction* function);
// create new function.
ObjFunction* newFunction();
// create new instance.
ObjInstance* newInstance(ObjClass* klass);
// create native function.
ObjNative* newNative(NativeFn function);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif