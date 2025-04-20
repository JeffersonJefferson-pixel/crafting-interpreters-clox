#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    // allocates an obj on the heap and initializes state
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    // insert allocated object to vm's linked list
    object->next = vm.objects;
    vm.objects = object;

    #ifdef DEBUG_LOG_GC
        printf("%p allocate %zu for %d\n", (void*)object, size, type);
    #endif

    return object;
}

ObjClass* newClass(ObjString* name) {
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name  = name;
    initTable(&klass->methods);
    return klass;
}

ObjClosure* newClosure(ObjFunction* function) {
    // allocate upvalue array
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);

    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjFunction* newFunction() {
    // allocate memory and initialize function object.
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjInstance* newInstance(ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    // init field table.
    initTable(&instance->fields);
    return instance;
}

ObjNative* newNative(NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

static ObjString* allocateString(char* chars, int length, uint32_t hash) {
    // create new ObjString on the heap and initalizes fields
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    // push string object to stack temporarily.
    push(OBJ_VAL(string));
    // set string in internal strings hash table.
    tableSet(&vm.strings, string, NIL_VAL);
    pop();
    return string;
}

static uint32_t hashString(const char* key, int length) {
    // fnv-1a hash function
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjString* takeString(char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    // check if string is interned.
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        // free memory since ownership is passed and no longer need the duplicate string.
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    // check if string is interned.
    ObjString * interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;

    // allocate new array on heap for characters and trailing terminator
    char* heapChars = ALLOCATE(char, length + 1);
    // copy characters from lexeme to array
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

static void printFunction(ObjFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

ObjUpvalue* newUpvalue(Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_CLASS:
            printf("%s", AS_CLASS(value)->name->chars);
            break;
        // handle closure object.
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        // handle function object.
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
    }
}