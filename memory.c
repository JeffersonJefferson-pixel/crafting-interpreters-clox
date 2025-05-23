#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    // trigger GC before allocation
    if (newSize > oldSize) {
        #ifdef DEBUG_STRESS_GC
            collectGarbage();
        #endif
    }
    // if (vm.bytesAllocated > vm.nextGC) {
    //     collectGarbage();
    // }

    vm.bytesAllocated = newSize - oldSize;
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }
    // everything is copied over.
    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

void markObject(Obj* object) {
    if (object == NULL) return;
    // don't mark already marked object to avoid loop.
    if (object->isMarked) return;
    // set flag.
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void *) object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif
    object->isMarked = true;

    // add gray objects to worklist.
    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        // calls system realloc.
        // gray stack not managed by GC.
        vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
        // abort on allocation failure
        if (vm.grayStack == NULL) exit(1); 
    }
    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
    // only mark object value since they are allocated on heap.
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}

static void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            markValue(bound->receiver);
            markObject((Obj*)bound->method);
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            markObject((Obj*)klass->name);
            markTable(&klass->methods);
            break;
        }
        case OBJ_CLOSURE: {
            // closure has reference to function it wraps and array of pointer to upvalues it captures.
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            // function has reference to string object containing the function's name.
            // function also has constant table.
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);
            markArray(&function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE: {
            // mark instance class and field table.
            ObjInstance* instance = (ObjInstance*)object;
            markObject((Obj*)instance->klass);
            markTable(&instance->fields);
            break;
        }
        case OBJ_UPVALUE:
            // trace reference to closed-over value from upvalue.
            markValue(((ObjUpvalue*)object)->closed);
            break;
        // native and string objects contain no outgoing references.
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

static void freeObject(Obj* object) {
    #ifdef DEBUG_LOG_GC
        printf("%p free type %d\n", (void*)object, object->type);
    #endif
    switch (object->type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            FREE(ObjBoundMethod, object);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            freeTable(&klass->methods);
            FREE(ObjClass, object);
            break;
        }
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
        // free instance object.
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            // free instance field table.
            freeTable(&instance->fields);
            FREE(ObjInstance, object);
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
    // free vm objects.
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
    // free vm gray stack.
    free(vm.grayStack);
}

static void markRoots() {
    // iterate roots in vm stack
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }

    // mark closure object in vm callframes.
    for (int i = 0; i < vm.frameCount; i++) {
        markObject((Obj*)vm.frames[i].closure);
    }

    // mark open upvalues in vm open upvalue list.
    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject((Obj*)upvalue);
    }

    // mark roots in global variables.
    markTable(&vm.globals);

    // compiler also uses memory from heap for literals and constant table.
    markCompilerRoots();
    markObject((Obj*)vm.initString);
}

static void traceReferences() {
    // iterate object in gray stack and mark as black.
    while (vm.grayCount > 0) {
        Obj* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

static void sweep() {
    Obj* previous = NULL;
    Obj* object = vm.objects;
    while (object != NULL) {
        if (object->isMarked) {
            // skip black object.
            // reset black object.
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            // unlink white object from the list
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                // free first node.
                vm.objects = object;
            }

            // free white object.
            freeObject(unreached);

        }
    }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
#endif

    markRoots();
    traceReferences();
    // remove string object in vm string table.
    tableRemoveWhite(&vm.strings);
    sweep();

    // adjust gc threshold.
    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu byte (from %zu to %zu) next at %zu\n", before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif
}