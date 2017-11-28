#ifndef _HASH_TAB_
#define _HASH_TAB_

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "cgraph.h"
#include "hashtab.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "pretty-print.h"
#include "hashtable.h"

/* Open addressing implementation */
typedef struct HashEntry {
    unsigned int hashVal;
    void *pData;
} HashEntry;

typedef struct HashTable {
    int tableSize;
    int entryCnt;
    HashEntry *mainEntry;
} HashTable;

typedef bool (*CmpFunc)(void *, void *);
typedef void (*DelFunc)();

HashTable *hashCreateTable(unsigned int tableSize);
void hashDeleteTable(HashTable *pTab, DelFunc doDel);
void *hashLookupTable(HashTable *pTab, unsigned int hashVal, CmpFunc isEqual,
    void *pData, bool doAdd);

#endif
