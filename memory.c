#include <stdlib.h>

#include "memory.h"
#include "vm.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }
    // everything is copied over.
    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

static void freeObject(Obj* object) {
    switch (object->type) {
        // handle closure object.
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            // free upvalue array.
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            // only free the closure object, and not the function object because the closure doesn't own the function.
            FREE(ObjClosure, object);
            break;
        }
        // handle function object.
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            // free function object's chunk.
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        // handle native function object.
        case OBJ_NATIVE: {
            FREE(ObjNative, object);
            break;
        }
        // handle string object.
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            // free string object's char array
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
    }
}

void freeObjects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    } 
}