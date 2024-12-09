#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"

VM vm;

static void resetStack() {
    vm.stackTop = vm.stack;
    // callframe stack is empty when vm starts up.
    vm.frameCount = 0;
}

void initVM() {
    resetStack();
    vm.objects = NULL;
    initTable(&vm.strings);
    // initialize global variable table.
    initTable(&vm.globals);
}

// clean up resources used by vm.
void freeVM() {
    // free global variable table.
    freeTable(&vm.globals);
    // free internal strings hash table.
    freeTable(&vm.strings);
    freeObjects();
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // stack trace
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->function;
        // line number curresponding to current ip.
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        }else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }

        resetStack();
    }

    // get topmost callframe.
    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    size_t instruction = frame->ip - frame->function->chunk.code - 1;
    int line = frame->function->chunk.lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool call(ObjFunction* function, int argCount) {
    // check number of argument against function arity.
    if (argCount != function->arity) {
        runtimeError("Expected %d arguments but got %d.", function->arity, argCount);
        return false;
    }

    // ensure call chain depth doesn't overflow callframe array.
    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }

    // inialize callframe on the stack.
    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;
    // minus 1 account for stack slot zero.
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}


static bool callValue(Value callee, int argCount) {
    // check if value being called is function object.
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_FUNCTION:
                return call(AS_FUNCTION(callee), argCount);
            default:
                break;
        }
    }
    runtimeError("Can only call functions and classes");
    return false;
}

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
    ObjString* b = AS_STRING(pop());
    ObjString* a = AS_STRING(pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));
}

InterpretResult interpret(const char* source) {
    // compile from source.
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    // push top-level function to vm stack.
    push(OBJ_VAL(function)); 
    // call the top-level function.
    call(function, 0);

    return run();
}

static InterpretResult run() {
    // get the topmost callframe.
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

    #define READ_BYTE() (*frame->ip++)
    #define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
    #define READ_SHORT() \
        (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
    #define READ_STRING() AS_STRING(READ_CONSTANT())
    #define BINARY_OP(valueType, op) \
        do { \
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
                runtimeError("Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            double b = AS_NUMBER(pop()); \
            double a = AS_NUMBER(pop()); \
            push (valueType(a op b)); \
        } while (false)

    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
            printf("          ");
            for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
                printf("[ ");
                printValue(*slot);
                printf(" ]");
            }
            printf("\n");
            disassembleInstruction(&frame->function->chunk, (int)(frame->ip - frame->function->chunk->code));
        #endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_POP: pop(); break;
            case OP_GET_LOCAL: {
                // get stack slot where local variable lives.
                uint8_t slot = READ_BYTE();
                // access local variable from current frame's slots array
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                // takes value from top of the stack 
                // and stores in stack slot corresponding to the local variable
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                // it doesn't pop the value.
                break;
            }
            case OP_GET_GLOBAL: {
                // get variable name.
                ObjString* name = READ_STRING();
                Value value;
                // look up variable's value by its name in global hash table. 
                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                // get name of variable from chunk constant table.
                ObjString* name = READ_STRING();
                // take value from top of stack and 
                // store it in a hash table with the name as key
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            };
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATE: 
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop()))); 
                break;
            case OP_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                // get function being called and number of arguments passed to the function.
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                // update current frame pointer. 
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_RETURN: {
                Value result = pop();
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    // exit interpreter.    
                    pop();
                    return INTERPRET_OK;
                }
                
                // discard callee slots and back at the beginning of the returning function's stack window.
                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_SHORT
    #undef READ_CONSTANT
    #undef READ_STRING
    #undef BINARY_OP
}