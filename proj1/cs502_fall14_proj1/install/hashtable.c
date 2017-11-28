#include <stdio.h>
#include <assert.h>
#include "hashtable.h"

/*
 * For any number, we can round it up to a number as
 * power of 2
 */
static hUpPow2(unsigned int val)
{
    val--;
    val |= val >> 1;
    val |= val >> 2;
    val |= val >> 4;
    val |= val >> 8;
    val |= val >> 16;
    val++;
    return val;
}

/*
 * Hashtable resize function implementation.
 */
static bool hashResizeTable(HashTable *pTab, unsigned int newSize)
{
    HashEntry *pNewEntry = (HashEntry *) xcalloc(newSize, sizeof(HashEntry));

    if (!pNewEntry)
        return false;

    int i;

    for (i=0; i<pTab->tableSize; i++) {

        void *pData = pTab->mainEntry[i].pData;

        if (pData) {
            unsigned int hashVal = pTab->mainEntry[i].hashVal;
            unsigned int newIdx =  hashVal & (newSize - 1);

            while (pNewEntry[newIdx].pData != NULL)
                newIdx = (newIdx + 1) & (newSize - 1);
            
            pNewEntry[newIdx].hashVal = hashVal;
            pNewEntry[newIdx].pData = pData;
        }
    }

    free(pTab->mainEntry);

    pTab->mainEntry = pNewEntry;
    pTab->tableSize = newSize;

    return true;
}

void hashDeleteTable(HashTable *pTab, DelFunc doDel)
{
    int i;

    for (i=0; i<pTab->tableSize; i++) {

        void *pData = pTab->mainEntry[i].pData;

        if (pData)
			doDel(pData);
    }

    free(pTab->mainEntry);
    free(pTab);
}
/*
 * Create a new hash table with specified table size
 */
HashTable *hashCreateTable(unsigned int tableSize)
{
    unsigned int allocSize;
    
    if (tableSize == 1)
        tableSize = 2;

    allocSize = hUpPow2(tableSize);

    HashTable *pTab = (HashTable *) xmalloc(sizeof(HashTable));

    pTab->mainEntry = (HashEntry *) xcalloc(allocSize, sizeof(HashEntry));

    pTab->tableSize = allocSize;

    pTab->entryCnt = 0;

    return pTab;
}

/* Hashtable lookup function implementation */
void *hashLookupTable(HashTable *pTab, unsigned int hashVal, CmpFunc isEqual,
    void *pData, bool doAdd)
{
    HashEntry *pEntry = &pTab->mainEntry[hashVal & (pTab->tableSize - 1)];
    HashEntry *pEnd = &pTab->mainEntry[pTab->tableSize];
    void *result = NULL;

    while (pEntry->pData && 
        (pEntry->hashVal != hashVal || !(*isEqual)(pEntry->pData, pData)))
    {

        pEntry++;
        
        if (pEntry == pEnd)
            pEntry = pTab->mainEntry;

    }
    
    /* Didn't find a match */
    if (!pEntry->pData) {
        if (doAdd) {
            pEntry->pData = pData;
            pEntry->hashVal = hashVal;
            pTab->entryCnt++;

            /* 
             * Resize the hashtable if load factor achieved.
             * Load factor is 75% by default
             */
            if (pTab->entryCnt * 4 > pTab->tableSize * 3) {
                /* Resizing myst work */
                assert(hashResizeTable(pTab, pTab->tableSize << 1));
            }
            
            result = pData;
        } else {
            /* result must be NULL*/
            assert(!result);
        }
    } else {
        /* Found match */
        result = pEntry->pData;
    }

    return result;
}
