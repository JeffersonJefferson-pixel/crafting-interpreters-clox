#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

// grow array when it is at least 75% full.
#define TABLE_MAX_LOAD 0.75

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
// retrieve value.
bool tableGet(Table* table, ObjString* key, Value* value);
// add key/value to hash table.
bool tableSet(Table* table, ObjString* key, Value value);
// copy all entries of one hash table to another.
void tableAddAll(Table* from, Table* to);

#endif