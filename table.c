#include <stdio.h>
#include <string.h>

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
    // bitmasking faster than modulo
    uint32_t index = key->hash & (capacity - 1);
    Entry* tombstone = NULL;
    // infinite loop is impossible assuming capacity is adjusted at max_load.
    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // empty entry, we are done.
                // if passed tombstone, reuse it.
                return tombstone != NULL ? tombstone: entry; 
            } else {
                // found a tombstone, continue.
                if (tombstone == NULL) tombstone = entry;
            }
        }
        // this works due to string interning.
        if (entry->key == key) {
            // found the entry
            return entry;
        }
        // probe due to collision.
        index = (index + 1) & (capacity - 1); // modulo operator to wrap back to the beginning.
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

    table->count = 0;

    // rebuild hash table due to new capacity.
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry * dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
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
    // increase count if new key and it goes to an empty bucket (not a tombstone).
    if (isNewKey && IS_NIL(entry->value)) table->count++;
    // set entry's key and value
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
    // check if table is empty.
    if (table->count == 0) return false;

    // find the entry.
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // place a tombstone in the entry.
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    // table's count doesn't decrease due to tombstone.
    return true;
}

void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    // similar to findEntry.
    // but this looks at actual strings.
    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length && 
            entry->key->hash == hash && 
            memcmp(entry->key->chars, chars, length) == 0) {
            // found match.
            return entry->key;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

void tableRemoveWhite(Table* table) {
    // iterate entries in table and delete value for unmarked key. 
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

void markTable(Table* table) {
    // iterate entries and mark key, value
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject((Obj*)entry->key);
        markValue(entry->value);
    }
}