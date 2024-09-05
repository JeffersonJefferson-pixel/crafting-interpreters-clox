#include "memory.h"
#include "table.h"

void initTable(Table* table) {
    // no allocation is done yet.
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

// find entry for either a lookup and an insert.
static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    // index where ideally we'll find the entry
    uint32_t index = key->hash % capacity;
    // infinite loop is impossible assuming capacity is adjusted at max_load.
    for (;;) {
        Entry* entry = &entries[index];
        // if bucket is empty or has the same key, we are done.
        if (entry->key == key || entry->key == NULL) {
            return entry;
        }
        // probe due to collision.
        index = (index + 1) % capacity; // modulo operator to wrap back to the beginning.
    }
}

static void adjustCapacity(Table* table, int capacity) {
    // create bucket array with capacity entries
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        // initialize entry
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    // rebuild hash table due to new capacity.
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry * dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
    }

    // release memory for the old array.
    FREE_ARRAY(Entry, table->entries, table->capacity);

    table->entries = entries;
    table->capacity = capacity;
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    // check if table is empty.
    if (table->count == 0) return false;
    // find entry.
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;
    // get value.
    *value = entry->value;
    return true;
}

bool tableSet(Table* table, ObjString* key, Value value) {
    // allocate entry array if necessary
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    // find entry in hash table by key
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    // increase count if new key
    if (isNewKey) table->count++;
    // set entry's key and value
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}