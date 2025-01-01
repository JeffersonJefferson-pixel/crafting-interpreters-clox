#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"
#include "object.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)


// a callframe represents a single ongoing function call.
typedef struct {
    ObjClosure* closure;
    uint8_t* ip; // caller's ip. when return from a function. the VM will jump to ip of the caller's callframe.
    Value* slots; // points to the VM's stack at the first slot this function can use.
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX]; // function calls have stack semantics.
    int frameCount; // stores current height of the callframe stack. it is the number of ongoing function calls.

    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals; // hash table for gloabl variables. 
    Table strings; // hash table of internal strings.
    Obj* objects;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif