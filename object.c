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

    // insert allocated object to vm's linked list
    object->next = vm.objects;
    vm.objects = object;

    return object;
}

ObjFunction* newFunction() {
    // allocate memory and initialize function object.
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

static ObjString* allocateString(char* chars, int length, uint32_t hash) {
    // create new ObjString on the heap and initalizes fields
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    // set string in internal strings hash table.
    tableSet(&vm.strings, string, NIL_VAL);
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

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        // handle function object.
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}