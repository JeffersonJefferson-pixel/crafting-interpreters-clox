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
    // number of entries and tombstones.
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
// remove an entry from hash table.
bool tableDelete(Table* table, ObjString* key);
// copy all entries of one hash table to another.
void tableAddAll(Table* from, Table* to);
// look for string in a hash table.
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

#endif