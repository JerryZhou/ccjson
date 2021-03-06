/*
  Copyright (c) 2015 JerryZhou (JerryZhou@outlook.com)
 
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
 
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/
#ifdef WIN32
// no warnings about the fopen_s, snprintf_s, bala bala in msvc
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>
#ifdef WIN32
#   include <windows.h>
#else
#   include <sys/time.h>
#   include <pthread.h>
#endif

#include "ccjson.h"

#ifdef WIN32
static int random() {
    return rand();
}
#endif

#define CC_STATIC static 

// Inernal Include dict 
// ******************************************************************************
// ******************************************************************************
#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)

typedef struct dictEntry {
    void *key;
    union {
        void *val;
        ccuint64 u64;
        ccint64 s64;
        double d;
    } v;
    struct dictEntry *next;
} dictEntry;

typedef struct dictType {
    unsigned int (*hashFunction)(const void *key);
    void *(*keyDup)(void *privdata, const void *key);
    void *(*valDup)(void *privdata, const void *obj);
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    void (*keyDestructor)(void *privdata, void *key);
    void (*valDestructor)(void *privdata, void *obj);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
typedef struct dictht {
    dictEntry **table;
    unsigned long size;
    unsigned long sizemask;
    unsigned long used;
} dictht;

typedef struct dict {
    dictType *type;
    void *privdata;
    dictht ht[2];
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
    int iterators; /* number of iterators currently running */
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. */
typedef struct dictIterator {
    dict *d;
    long index;
    int table, safe;
    dictEntry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);

/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        entry->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        entry->v.val = (_val_); \
} while(0)

#define dictSetSignedIntegerVal(entry, _val_) \
    do { entry->v.s64 = _val_; } while(0)

#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { entry->v.u64 = _val_; } while(0)

#define dictSetDoubleVal(entry, _val_) \
    do { entry->v.d = _val_; } while(0)

#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        entry->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        entry->key = (_key_); \
} while(0)

#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

#define dictHashKey(d, key) (d)->type->hashFunction(key)
#define dictGetKey(he) ((he)->key)
#define dictGetVal(he) ((he)->v.val)
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
#define dictGetDoubleVal(he) ((he)->v.d)
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
#define dictIsRehashing(d) ((d)->rehashidx != -1)

/* API */
CC_STATIC dict *dictCreate(dictType *type, void *privDataPtr);
CC_STATIC int dictExpand(dict *d, unsigned long size);
CC_STATIC int dictAdd(dict *d, void *key, void *val);
CC_STATIC dictEntry *dictAddRaw(dict *d, void *key);
CC_STATIC int dictReplace(dict *d, void *key, void *val);
CC_STATIC dictEntry *dictReplaceRaw(dict *d, void *key);
CC_STATIC int dictDelete(dict *d, const void *key);
CC_STATIC int dictDeleteNoFree(dict *d, const void *key);
CC_STATIC void dictRelease(dict *d);
CC_STATIC dictEntry * dictFind(dict *d, const void *key);
CC_STATIC void *dictFetchValue(dict *d, const void *key);
CC_STATIC int dictResize(dict *d);
CC_STATIC dictIterator *dictGetIterator(dict *d);
CC_STATIC dictIterator *dictGetSafeIterator(dict *d);
CC_STATIC dictEntry *dictNext(dictIterator *iter);
CC_STATIC void dictReleaseIterator(dictIterator *iter);
CC_STATIC dictEntry *dictGetRandomKey(dict *d);
CC_STATIC unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);
CC_STATIC void dictPrintStats(dict *d);
CC_STATIC unsigned int dictGenHashFunction(const void *key, int len);
CC_STATIC unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len);
CC_STATIC void dictEmpty(dict *d, void(callback)(void*));
CC_STATIC void dictEnableResize(void);
CC_STATIC void dictDisableResize(void);
CC_STATIC int dictRehash(dict *d, int n);
CC_STATIC int dictRehashMilliseconds(dict *d, int ms);
CC_STATIC void dictSetHashFunctionSeed(unsigned int initval);
CC_STATIC unsigned int dictGetHashFunctionSeed(void);
CC_STATIC unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, void *privdata);

/* Hash table types */
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#define zmalloc malloc
#define zcalloc(size) calloc(1, size)
#define zfree free

/* Using dictEnableResize() / dictDisableResize() we make possible to
 * enable/disable resizing of the hash table as needed. This is very important
 * for Redis, as we use copy-on-write and don't want to move too much memory
 * around when there is a child performing saving operations.
 *
 * Note that even when dict_can_resize is set to 0, not all resizes are
 * prevented: a hash table is still allowed to grow if the ratio between
 * the number of elements and the buckets > dict_force_resize_ratio. */
static int dict_can_resize = 1;
static unsigned int dict_force_resize_ratio = 5;

/* -------------------------- private prototypes ---------------------------- */

static int _dictExpandIfNeeded(dict *ht);
static unsigned long _dictNextPower(unsigned long size);
static int _dictKeyIndex(dict *ht, const void *key);
static int _dictInit(dict *ht, dictType *type, void *privDataPtr);

/* -------------------------- hash functions -------------------------------- */

/* Thomas Wang's 32 bit Mix Function */
CC_STATIC unsigned int dictIntHashFunction(unsigned int key)
{
    key += ~(key << 15);
    key ^=  (key >> 10);
    key +=  (key << 3);
    key ^=  (key >> 6);
    key += ~(key << 11);
    key ^=  (key >> 16);
    return key;
}

static ccuint32 dict_hash_function_seed = 5381;

CC_STATIC void dictSetHashFunctionSeed(ccuint32 seed) {
    dict_hash_function_seed = seed;
}

CC_STATIC ccuint32 dictGetHashFunctionSeed(void) {
    return dict_hash_function_seed;
}

/* MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 * 2. sizeof(int) == 4
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */
CC_STATIC unsigned int dictGenHashFunction(const void *key, int len) {
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    ccuint32 seed = dict_hash_function_seed;
    const ccuint32 m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    ccuint32 h = seed ^ len;

    /* Mix 4 bytes at a time into the hash */
    const unsigned char *data = (const unsigned char *)key;

    while(len >= 4) {
        ccuint32 k = *(ccuint32*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch(len) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0]; h *= m;
    };

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return (unsigned int)h;
}

/* And a case insensitive hash function (based on djb hash) */
CC_STATIC unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len) {
    unsigned int hash = (unsigned int)dict_hash_function_seed;

    while (len--)
        hash = ((hash << 5) + hash) + (tolower(*buf++)); /* hash * 33 + c */
    return hash;
}

/* ----------------------------- API implementation ------------------------- */

/* Reset a hash table already initialized with ht_init().
 * NOTE: This function should only be called by ht_destroy(). */
static void _dictReset(dictht *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

/* Create a new hash table */
CC_STATIC dict *dictCreate(dictType *type,
        void *privDataPtr)
{
    dict *d = zmalloc(sizeof(*d));

    _dictInit(d,type,privDataPtr);
    return d;
}

/* Initialize the hash table */
int _dictInit(dict *d, dictType *type,
        void *privDataPtr)
{
    _dictReset(&d->ht[0]);
    _dictReset(&d->ht[1]);
    d->type = type;
    d->privdata = privDataPtr;
    d->rehashidx = -1;
    d->iterators = 0;
    return DICT_OK;
}

/* Resize the table to the minimal size that contains all the elements,
 * but with the invariant of a USED/BUCKETS ratio near to <= 1 */
CC_STATIC int dictResize(dict *d)
{
    int minimal;

    if (!dict_can_resize || dictIsRehashing(d)) return DICT_ERR;
    minimal = (int)d->ht[0].used;
    if (minimal < DICT_HT_INITIAL_SIZE)
        minimal = DICT_HT_INITIAL_SIZE;
    return dictExpand(d, minimal);
}

/* Expand or create the hash table */
CC_STATIC int dictExpand(dict *d, unsigned long size)
{
    dictht n; /* the new hash table */
    unsigned long realsize = _dictNextPower(size);

    /* the size is invalid if it is smaller than the number of
     * elements already inside the hash table */
    if (dictIsRehashing(d) || d->ht[0].used > size)
        return DICT_ERR;

    /* Rehashing to the same table size is not useful. */
    if (realsize == d->ht[0].size) return DICT_ERR;

    /* Allocate the new hash table and initialize all pointers to NULL */
    n.size = realsize;
    n.sizemask = realsize-1;
    n.table = zcalloc(realsize*sizeof(dictEntry*));
    n.used = 0;

    /* Is this the first initialization? If so it's not really a rehashing
     * we just set the first hash table so that it can accept keys. */
    if (d->ht[0].table == NULL) {
        d->ht[0] = n;
        return DICT_OK;
    }

    /* Prepare a second hash table for incremental rehashing */
    d->ht[1] = n;
    d->rehashidx = 0;
    return DICT_OK;
}

/* Performs N steps of incremental rehashing. Returns 1 if there are still
 * keys to move from the old to the new hash table, otherwise 0 is returned.
 *
 * Note that a rehashing step consists in moving a bucket (that may have more
 * than one key as we use chaining) from the old to the new hash table, however
 * since part of the hash table may be composed of empty spaces, it is not
 * guaranteed that this function will rehash even a single bucket, since it
 * will visit at max N*10 empty buckets in total, otherwise the amount of
 * work it does would be unbound and the function may block for a long time. */
CC_STATIC int dictRehash(dict *d, int n) {
    int empty_visits = n*10; /* Max number of empty buckets to visit. */
    if (!dictIsRehashing(d)) return 0;

    while(n-- && d->ht[0].used != 0) {
        dictEntry *de, *nextde;

        /* Note that rehashidx can't overflow as we are sure there are more
         * elements because ht[0].used != 0 */
        assert(d->ht[0].size > (unsigned long)d->rehashidx);
        while(d->ht[0].table[d->rehashidx] == NULL) {
            d->rehashidx++;
            if (--empty_visits == 0) return 1;
        }
        de = d->ht[0].table[d->rehashidx];
        /* Move all the keys in this bucket from the old to the new hash HT */
        while(de) {
            unsigned int h;

            nextde = de->next;
            /* Get the index in the new hash table */
            h = dictHashKey(d, de->key) & d->ht[1].sizemask;
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;
            d->ht[0].used--;
            d->ht[1].used++;
            de = nextde;
        }
        d->ht[0].table[d->rehashidx] = NULL;
        d->rehashidx++;
    }

    /* Check if we already rehashed the whole table... */
    if (d->ht[0].used == 0) {
        zfree(d->ht[0].table);
        d->ht[0] = d->ht[1];
        _dictReset(&d->ht[1]);
        d->rehashidx = -1;
        return 0;
    }

    /* More to rehash... */
    return 1;
}

CC_STATIC long long timeInMilliseconds(void) {
    return ccgetcurnano() / 1000;
}

/* Rehash for an amount of time between ms milliseconds and ms+1 milliseconds */
CC_STATIC int dictRehashMilliseconds(dict *d, int ms) {
    long long start = timeInMilliseconds();
    int rehashes = 0;

    while(dictRehash(d,100)) {
        rehashes += 100;
        if (timeInMilliseconds()-start > ms) break;
    }
    return rehashes;
}

/* This function performs just a step of rehashing, and only if there are
 * no safe iterators bound to our hash table. When we have iterators in the
 * middle of a rehashing we can't mess with the two hash tables otherwise
 * some element can be missed or duplicated.
 *
 * This function is called by common lookup or update operations in the
 * dictionary so that the hash table automatically migrates from H1 to H2
 * while it is actively used. */
static void _dictRehashStep(dict *d) {
    if (d->iterators == 0) dictRehash(d,1);
}

/* Add an element to the target hash table */
CC_STATIC int dictAdd(dict *d, void *key, void *val)
{
    dictEntry *entry = dictAddRaw(d,key);

    if (!entry) return DICT_ERR;
    dictSetVal(d, entry, val);
    return DICT_OK;
}

/* Low level add. This function adds the entry but instead of setting
 * a value returns the dictEntry structure to the user, that will make
 * sure to fill the value field as he wishes.
 *
 * This function is also directly exposed to the user API to be called
 * mainly in order to store non-pointers inside the hash value, example:
 *
 * entry = dictAddRaw(dict,mykey);
 * if (entry != NULL) dictSetSignedIntegerVal(entry,1000);
 *
 * Return values:
 *
 * If key already exists NULL is returned.
 * If key was added, the hash entry is returned to be manipulated by the caller.
 */
CC_STATIC dictEntry *dictAddRaw(dict *d, void *key)
{
    int index;
    dictEntry *entry;
    dictht *ht;

    if (dictIsRehashing(d)) _dictRehashStep(d);

    /* Get the index of the new element, or -1 if
     * the element already exists. */
    if ((index = _dictKeyIndex(d, key)) == -1)
        return NULL;

    /* Allocate the memory and store the new entry.
     * Insert the element in top, with the assumption that in a database
     * system it is more likely that recently added entries are accessed
     * more frequently. */
    ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];
    entry = zmalloc(sizeof(*entry));
    entry->next = ht->table[index];
    ht->table[index] = entry;
    ht->used++;

    /* Set the hash entry fields. */
    dictSetKey(d, entry, key);
    return entry;
}

/* Add an element, discarding the old if the key already exists.
 * Return 1 if the key was added from scratch, 0 if there was already an
 * element with such key and dictReplace() just performed a value update
 * operation. */
CC_STATIC int dictReplace(dict *d, void *key, void *val)
{
    dictEntry *entry, auxentry;

    /* Try to add the element. If the key
     * does not exists dictAdd will suceed. */
    if (dictAdd(d, key, val) == DICT_OK)
        return 1;
    /* It already exists, get the entry */
    entry = dictFind(d, key);
    /* Set the new value and free the old one. Note that it is important
     * to do that in this order, as the value may just be exactly the same
     * as the previous one. In this context, think to reference counting,
     * you want to increment (set), and then decrement (free), and not the
     * reverse. */
    auxentry = *entry;
    dictSetVal(d, entry, val);
    dictFreeVal(d, &auxentry);
    return 0;
}

/* dictReplaceRaw() is simply a version of dictAddRaw() that always
 * returns the hash entry of the specified key, even if the key already
 * exists and can't be added (in that case the entry of the already
 * existing key is returned.)
 *
 * See dictAddRaw() for more information. */
CC_STATIC dictEntry *dictReplaceRaw(dict *d, void *key) {
    dictEntry *entry = dictFind(d,key);

    return entry ? entry : dictAddRaw(d,key);
}

/* Search and remove an element */
static int dictGenericDelete(dict *d, const void *key, int nofree)
{
    unsigned int h, idx;
    dictEntry *he, *prevHe;
    int table;

    if (d->ht[0].size == 0) return DICT_ERR; /* d->ht[0].table is NULL */
    if (dictIsRehashing(d)) _dictRehashStep(d);
    h = dictHashKey(d, key);

    for (table = 0; table <= 1; table++) {
        idx = h & d->ht[table].sizemask;
        he = d->ht[table].table[idx];
        prevHe = NULL;
        while(he) {
            if (dictCompareKeys(d, key, he->key)) {
                /* Unlink the element from the list */
                if (prevHe)
                    prevHe->next = he->next;
                else
                    d->ht[table].table[idx] = he->next;
                if (!nofree) {
                    dictFreeKey(d, he);
                    dictFreeVal(d, he);
                }
                zfree(he);
                d->ht[table].used--;
                return DICT_OK;
            }
            prevHe = he;
            he = he->next;
        }
        if (!dictIsRehashing(d)) break;
    }
    return DICT_ERR; /* not found */
}

CC_STATIC int dictDelete(dict *ht, const void *key) {
    return dictGenericDelete(ht,key,0);
}

CC_STATIC int dictDeleteNoFree(dict *ht, const void *key) {
    return dictGenericDelete(ht,key,1);
}

/* Destroy an entire dictionary */
int _dictClear(dict *d, dictht *ht, void(callback)(void *)) {
    unsigned long i;

    /* Free all the elements */
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        dictEntry *he, *nextHe;

        if (callback && (i & 65535) == 0) callback(d->privdata);

        if ((he = ht->table[i]) == NULL) continue;
        while(he) {
            nextHe = he->next;
            dictFreeKey(d, he);
            dictFreeVal(d, he);
            zfree(he);
            ht->used--;
            he = nextHe;
        }
    }
    /* Free the table and the allocated cache structure */
    zfree(ht->table);
    /* Re-initialize the table */
    _dictReset(ht);
    return DICT_OK; /* never fails */
}

/* Clear & Release the hash table */
CC_STATIC void dictRelease(dict *d)
{
    _dictClear(d,&d->ht[0],NULL);
    _dictClear(d,&d->ht[1],NULL);
    zfree(d);
}

CC_STATIC dictEntry *dictFind(dict *d, const void *key)
{
    dictEntry *he;
    unsigned int h, idx, table;

    if (d->ht[0].size == 0) return NULL; /* We don't have a table at all */
    if (dictIsRehashing(d)) _dictRehashStep(d);
    h = dictHashKey(d, key);
    for (table = 0; table <= 1; table++) {
        idx = h & d->ht[table].sizemask;
        he = d->ht[table].table[idx];
        while(he) {
            if (dictCompareKeys(d, key, he->key))
                return he;
            he = he->next;
        }
        if (!dictIsRehashing(d)) return NULL;
    }
    return NULL;
}

CC_STATIC void *dictFetchValue(dict *d, const void *key) {
    dictEntry *he;

    he = dictFind(d,key);
    return he ? dictGetVal(he) : NULL;
}

/* A fingerprint is a 64 bit number that represents the state of the dictionary
 * at a given time, it's just a few dict properties xored together.
 * When an unsafe iterator is initialized, we get the dict fingerprint, and check
 * the fingerprint again when the iterator is released.
 * If the two fingerprints are different it means that the user of the iterator
 * performed forbidden operations against the dictionary while iterating. */
CC_STATIC long long dictFingerprint(dict *d) {
    long long integers[6], hash = 0;
    int j;

    integers[0] = (long) d->ht[0].table;
    integers[1] = d->ht[0].size;
    integers[2] = d->ht[0].used;
    integers[3] = (long) d->ht[1].table;
    integers[4] = d->ht[1].size;
    integers[5] = d->ht[1].used;

    /* We hash N integers by summing every successive integer with the integer
     * hashing of the previous sum. Basically:
     *
     * Result = hash(hash(hash(int1)+int2)+int3) ...
     *
     * This way the same set of integers in a different order will (likely) hash
     * to a different number. */
    for (j = 0; j < 6; j++) {
        hash += integers[j];
        /* For the hashing step we use Tomas Wang's 64 bit integer hash. */
        hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
    }
    return hash;
}

CC_STATIC dictIterator *dictGetIterator(dict *d)
{
    dictIterator *iter = zmalloc(sizeof(*iter));

    iter->d = d;
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    iter->nextEntry = NULL;
    return iter;
}

CC_STATIC dictIterator *dictGetSafeIterator(dict *d) {
    dictIterator *i = dictGetIterator(d);

    i->safe = 1;
    return i;
}

CC_STATIC dictEntry *dictNext(dictIterator *iter)
{
    while (1) {
        if (iter->entry == NULL) {
            dictht *ht = &iter->d->ht[iter->table];
            if (iter->index == -1 && iter->table == 0) {
                if (iter->safe)
                    iter->d->iterators++;
                else
                    iter->fingerprint = dictFingerprint(iter->d);
            }
            iter->index++;
            if (iter->index >= (long) ht->size) {
                if (dictIsRehashing(iter->d) && iter->table == 0) {
                    iter->table++;
                    iter->index = 0;
                    ht = &iter->d->ht[1];
                } else {
                    break;
                }
            }
            iter->entry = ht->table[iter->index];
        } else {
            iter->entry = iter->nextEntry;
        }
        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            iter->nextEntry = iter->entry->next;
            return iter->entry;
        }
    }
    return NULL;
}

CC_STATIC void dictReleaseIterator(dictIterator *iter)
{
    if (!(iter->index == -1 && iter->table == 0)) {
        if (iter->safe)
            iter->d->iterators--;
        else
            assert(iter->fingerprint == dictFingerprint(iter->d));
    }
    zfree(iter);
}

/* Return a random entry from the hash table. Useful to
 * implement randomized algorithms */
CC_STATIC dictEntry *dictGetRandomKey(dict *d)
{
    dictEntry *he, *orighe;
    unsigned int h;
    int listlen, listele;

    if (dictSize(d) == 0) return NULL;
    if (dictIsRehashing(d)) _dictRehashStep(d);
    if (dictIsRehashing(d)) {
        do {
            /* We are sure there are no elements in indexes from 0
             * to rehashidx-1 */
            h = (unsigned int)(d->rehashidx + (random() % (d->ht[0].size +
                                            d->ht[1].size -
                                            d->rehashidx)));
            he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] :
                                      d->ht[0].table[h];
        } while(he == NULL);
    } else {
        do {
            h = (unsigned int)(random() & d->ht[0].sizemask);
            he = d->ht[0].table[h];
        } while(he == NULL);
    }

    /* Now we found a non empty bucket, but it is a linked
     * list and we need to get a random element from the list.
     * The only sane way to do so is counting the elements and
     * select a random index. */
    listlen = 0;
    orighe = he;
    while(he) {
        he = he->next;
        listlen++;
    }
    listele = random() % listlen;
    he = orighe;
    while(listele--) he = he->next;
    return he;
}

/* This function samples the dictionary to return a few keys from random
 * locations.
 *
 * It does not guarantee to return all the keys specified in 'count', nor
 * it does guarantee to return non-duplicated elements, however it will make
 * some effort to do both things.
 *
 * Returned pointers to hash table entries are stored into 'des' that
 * points to an array of dictEntry pointers. The array must have room for
 * at least 'count' elements, that is the argument we pass to the function
 * to tell how many random elements we need.
 *
 * The function returns the number of items stored into 'des', that may
 * be less than 'count' if the hash table has less than 'count' elements
 * inside, or if not enough elements were found in a reasonable amount of
 * steps.
 *
 * Note that this function is not suitable when you need a good distribution
 * of the returned items, but only when you need to "sample" a given number
 * of continuous elements to run some kind of algorithm or to produce
 * statistics. However the function is much faster than dictGetRandomKey()
 * at producing N elements. */
CC_STATIC unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count) {
    unsigned int j; /* internal hash table id, 0 or 1. */
    unsigned int tables; /* 1 or 2 tables? */
    unsigned int stored = 0, maxsizemask;
    unsigned int maxsteps;
    unsigned int i;
    unsigned int emptylen;
    dictEntry *he;

    if ((unsigned int)dictSize(d) < count) count = (unsigned int)dictSize(d);
    maxsteps = count*10;

    /* Try to do a rehashing work proportional to 'count'. */
    for (j = 0; j < count; j++) {
        if (dictIsRehashing(d))
            _dictRehashStep(d);
        else
            break;
    }

    tables = dictIsRehashing(d) ? 2 : 1;
    maxsizemask = (unsigned int)d->ht[0].sizemask;
    if (tables > 1 && maxsizemask < d->ht[1].sizemask)
        maxsizemask = (unsigned int)d->ht[1].sizemask;

    /* Pick a random point inside the larger table. */
    i = random() & maxsizemask;
    emptylen = 0; /* Continuous empty entries so far. */
    while(stored < count && maxsteps--) {
        for (j = 0; j < tables; j++) {
            /* Invariant of the dict.c rehashing: up to the indexes already
             * visited in ht[0] during the rehashing, there are no populated
             * buckets, so we can skip ht[0] for indexes between 0 and idx-1. */
            if (tables == 2 && j == 0 && i < (unsigned int)d->rehashidx) {
                /* Moreover, if we are currently out of range in the second
                 * table, there will be no elements in both tables up to
                 * the current rehashing index, so we jump if possible.
                 * (this happens when going from big to small table). */
                if (i >= d->ht[1].size) i = (unsigned int)d->rehashidx;
                continue;
            }
            if (i >= d->ht[j].size) continue; /* Out of range for this table. */
            he = d->ht[j].table[i];

            /* Count contiguous empty buckets, and jump to other
             * locations if they reach 'count' (with a minimum of 5). */
            if (he == NULL) {
                emptylen++;
                if (emptylen >= 5 && emptylen > count) {
                    i = random() & maxsizemask;
                    emptylen = 0;
                }
            } else {
                emptylen = 0;
                while (he) {
                    /* Collect all the elements of the buckets found non
                     * empty while iterating. */
                    *des = he;
                    des++;
                    he = he->next;
                    stored++;
                    if (stored == count) return stored;
                }
            }
        }
        i = (i+1) & maxsizemask;
    }
    return stored;
}

/* Function to reverse bits. Algorithm from:
 * http://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel */
static unsigned long rev(unsigned long v) {
    unsigned long s = 8 * sizeof(v); // bit size; must be power of 2
    unsigned long mask = ~0;
    while ((s >>= 1) > 0) {
        mask ^= (mask << s);
        v = ((v >> s) & mask) | ((v << s) & ~mask);
    }
    return v;
}

/* dictScan() is used to iterate over the elements of a dictionary.
 *
 * Iterating works the following way:
 *
 * 1) Initially you call the function using a cursor (v) value of 0.
 * 2) The function performs one step of the iteration, and returns the
 *    new cursor value you must use in the next call.
 * 3) When the returned cursor is 0, the iteration is complete.
 *
 * The function guarantees all elements present in the
 * dictionary get returned between the start and end of the iteration.
 * However it is possible some elements get returned multiple times.
 *
 * For every element returned, the callback argument 'fn' is
 * called with 'privdata' as first argument and the dictionary entry
 * 'de' as second argument.
 *
 * HOW IT WORKS.
 *
 * The iteration algorithm was designed by Pieter Noordhuis.
 * The main idea is to increment a cursor starting from the higher order
 * bits. That is, instead of incrementing the cursor normally, the bits
 * of the cursor are reversed, then the cursor is incremented, and finally
 * the bits are reversed again.
 *
 * This strategy is needed because the hash table may be resized between
 * iteration calls.
 *
 * dict.c hash tables are always power of two in size, and they
 * use chaining, so the position of an element in a given table is given
 * by computing the bitwise AND between Hash(key) and SIZE-1
 * (where SIZE-1 is always the mask that is equivalent to taking the rest
 *  of the division between the Hash of the key and SIZE).
 *
 * For example if the current hash table size is 16, the mask is
 * (in binary) 1111. The position of a key in the hash table will always be
 * the last four bits of the hash output, and so forth.
 *
 * WHAT HAPPENS IF THE TABLE CHANGES IN SIZE?
 *
 * If the hash table grows, elements can go anywhere in one multiple of
 * the old bucket: for example let's say we already iterated with
 * a 4 bit cursor 1100 (the mask is 1111 because hash table size = 16).
 *
 * If the hash table will be resized to 64 elements, then the new mask will
 * be 111111. The new buckets you obtain by substituting in ??1100
 * with either 0 or 1 can be targeted only by keys we already visited
 * when scanning the bucket 1100 in the smaller hash table.
 *
 * By iterating the higher bits first, because of the inverted counter, the
 * cursor does not need to restart if the table size gets bigger. It will
 * continue iterating using cursors without '1100' at the end, and also
 * without any other combination of the final 4 bits already explored.
 *
 * Similarly when the table size shrinks over time, for example going from
 * 16 to 8, if a combination of the lower three bits (the mask for size 8
 * is 111) were already completely explored, it would not be visited again
 * because we are sure we tried, for example, both 0111 and 1111 (all the
 * variations of the higher bit) so we don't need to test it again.
 *
 * WAIT... YOU HAVE *TWO* TABLES DURING REHASHING!
 *
 * Yes, this is true, but we always iterate the smaller table first, then
 * we test all the expansions of the current cursor into the larger
 * table. For example if the current cursor is 101 and we also have a
 * larger table of size 16, we also test (0)101 and (1)101 inside the larger
 * table. This reduces the problem back to having only one table, where
 * the larger one, if it exists, is just an expansion of the smaller one.
 *
 * LIMITATIONS
 *
 * This iterator is completely stateless, and this is a huge advantage,
 * including no additional memory used.
 *
 * The disadvantages resulting from this design are:
 *
 * 1) It is possible we return elements more than once. However this is usually
 *    easy to deal with in the application level.
 * 2) The iterator must return multiple elements per call, as it needs to always
 *    return all the keys chained in a given bucket, and all the expansions, so
 *    we are sure we don't miss keys moving during rehashing.
 * 3) The reverse cursor is somewhat hard to understand at first, but this
 *    comment is supposed to help.
 */
CC_STATIC unsigned long dictScan(dict *d,
                       unsigned long v,
                       dictScanFunction *fn,
                       void *privdata)
{
    dictht *t0, *t1;
    const dictEntry *de;
    unsigned long m0, m1;

    if (dictSize(d) == 0) return 0;

    if (!dictIsRehashing(d)) {
        t0 = &(d->ht[0]);
        m0 = t0->sizemask;

        /* Emit entries at cursor */
        de = t0->table[v & m0];
        while (de) {
            fn(privdata, de);
            de = de->next;
        }

    } else {
        t0 = &d->ht[0];
        t1 = &d->ht[1];

        /* Make sure t0 is the smaller and t1 is the bigger table */
        if (t0->size > t1->size) {
            t0 = &d->ht[1];
            t1 = &d->ht[0];
        }

        m0 = t0->sizemask;
        m1 = t1->sizemask;

        /* Emit entries at cursor */
        de = t0->table[v & m0];
        while (de) {
            fn(privdata, de);
            de = de->next;
        }

        /* Iterate over indices in larger table that are the expansion
         * of the index pointed to by the cursor in the smaller table */
        do {
            /* Emit entries at cursor */
            de = t1->table[v & m1];
            while (de) {
                fn(privdata, de);
                de = de->next;
            }

            /* Increment bits not covered by the smaller mask */
            v = (((v | m0) + 1) & ~m0) | (v & m0);

            /* Continue while bits covered by mask difference is non-zero */
        } while (v & (m0 ^ m1));
    }

    /* Set unmasked bits so incrementing the reversed cursor
     * operates on the masked bits of the smaller table */
    v |= ~m0;

    /* Increment the reverse cursor */
    v = rev(v);
    v++;
    v = rev(v);

    return v;
}

/* ------------------------- private functions ------------------------------ */

/* Expand the hash table if needed */
static int _dictExpandIfNeeded(dict *d)
{
    /* Incremental rehashing already in progress. Return. */
    if (dictIsRehashing(d)) return DICT_OK;

    /* If the hash table is empty expand it to the initial size. */
    if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);

    /* If we reached the 1:1 ratio, and we are allowed to resize the hash
     * table (global setting) or we should avoid it but the ratio between
     * elements/buckets is over the "safe" threshold, we resize doubling
     * the number of buckets. */
    if (d->ht[0].used >= d->ht[0].size &&
        (dict_can_resize ||
         d->ht[0].used/d->ht[0].size > dict_force_resize_ratio))
    {
        return dictExpand(d, d->ht[0].used*2);
    }
    return DICT_OK;
}

/* Our hash table capability is a power of two */
static unsigned long _dictNextPower(unsigned long size)
{
    unsigned long i = DICT_HT_INITIAL_SIZE;

    if (size >= LONG_MAX) return LONG_MAX;
    while(1) {
        if (i >= size)
            return i;
        i *= 2;
    }
}

/* Returns the index of a free slot that can be populated with
 * a hash entry for the given 'key'.
 * If the key already exists, -1 is returned.
 *
 * Note that if we are in the process of rehashing the hash table, the
 * index is always returned in the context of the second (new) hash table. */
static int _dictKeyIndex(dict *d, const void *key)
{
    unsigned int h, idx, table;
    dictEntry *he;

    /* Expand the hash table if needed */
    if (_dictExpandIfNeeded(d) == DICT_ERR)
        return -1;
    /* Compute the key hash value */
    h = dictHashKey(d, key);
    for (table = 0; table <= 1; table++) {
        idx = h & d->ht[table].sizemask;
        /* Search if this slot does not already contain the given key */
        he = d->ht[table].table[idx];
        while(he) {
            if (dictCompareKeys(d, key, he->key))
                return -1;
            he = he->next;
        }
        if (!dictIsRehashing(d)) break;
    }
    return idx;
}

CC_STATIC void dictEmpty(dict *d, void(callback)(void*)) {
    _dictClear(d,&d->ht[0],callback);
    _dictClear(d,&d->ht[1],callback);
    d->rehashidx = -1;
    d->iterators = 0;
}

CC_STATIC void dictEnableResize(void) {
    dict_can_resize = 1;
}

CC_STATIC void dictDisableResize(void) {
    dict_can_resize = 0;
}

#if 0
/* The following is code that we don't use for Redis currently, but that is part
of the library. */

/* ----------------------- Debugging ------------------------*/

#define DICT_STATS_VECTLEN 50
static void _dictPrintStatsHt(dictht *ht) {
    unsigned long i, slots = 0, chainlen, maxchainlen = 0;
    unsigned long totchainlen = 0;
    unsigned long clvector[DICT_STATS_VECTLEN];

    if (ht->used == 0) {
        printf("No stats available for empty dictionaries\n");
        return;
    }

    for (i = 0; i < DICT_STATS_VECTLEN; i++) clvector[i] = 0;
    for (i = 0; i < ht->size; i++) {
        dictEntry *he;

        if (ht->table[i] == NULL) {
            clvector[0]++;
            continue;
        }
        slots++;
        /* For each hash entry on this slot... */
        chainlen = 0;
        he = ht->table[i];
        while(he) {
            chainlen++;
            he = he->next;
        }
        clvector[(chainlen < DICT_STATS_VECTLEN) ? chainlen : (DICT_STATS_VECTLEN-1)]++;
        if (chainlen > maxchainlen) maxchainlen = chainlen;
        totchainlen += chainlen;
    }
    printf("Hash table stats:\n");
    printf(" table size: %ld\n", ht->size);
    printf(" number of elements: %ld\n", ht->used);
    printf(" different slots: %ld\n", slots);
    printf(" max chain length: %ld\n", maxchainlen);
    printf(" avg chain length (counted): %.02f\n", (float)totchainlen/slots);
    printf(" avg chain length (computed): %.02f\n", (float)ht->used/slots);
    printf(" Chain length distribution:\n");
    for (i = 0; i < DICT_STATS_VECTLEN-1; i++) {
        if (clvector[i] == 0) continue;
        printf("   %s%ld: %ld (%.02f%%)\n",(i == DICT_STATS_VECTLEN-1)?">= ":"", i, clvector[i], ((float)clvector[i]/ht->size)*100);
    }
}

void dictPrintStats(dict *d) {
    _dictPrintStatsHt(&d->ht[0]);
    if (dictIsRehashing(d)) {
        printf("-- Rehashing into ht[1]:\n");
        _dictPrintStatsHt(&d->ht[1]);
    }
}

/* ----------------------- StringCopy Hash Table Type ------------------------*/

static unsigned int _dictStringCopyHTHashFunction(const void *key)
{
    return dictGenHashFunction(key, strlen(key));
}

static void *_dictStringDup(void *privdata, const void *key)
{
    int len = strlen(key);
    char *copy = zmalloc(len+1);
    DICT_NOTUSED(privdata);

    memcpy(copy, key, len);
    copy[len] = '\0';
    return copy;
}

static int _dictStringCopyHTKeyCompare(void *privdata, const void *key1,
        const void *key2)
{
    DICT_NOTUSED(privdata);

    return strcmp(key1, key2) == 0;
}

static void _dictStringDestructor(void *privdata, void *key)
{
    DICT_NOTUSED(privdata);

    zfree(key);
}

dictType dictTypeHeapStringCopyKey = {
    _dictStringCopyHTHashFunction, /* hash function */
    _dictStringDup,                /* key dup */
    NULL,                          /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    NULL                           /* val destructor */
};

/* This is like StringCopy but does not auto-duplicate the key.
 * It's used for intepreter's shared strings. */
dictType dictTypeHeapStrings = {
    _dictStringCopyHTHashFunction, /* hash function */
    NULL,                          /* key dup */
    NULL,                          /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    NULL                           /* val destructor */
};

/* This is like StringCopy but also automatically handle dynamic
 * allocated C strings as values. */
dictType dictTypeHeapStringCopyKeyValue = {
    _dictStringCopyHTHashFunction, /* hash function */
    _dictStringDup,                /* key dup */
    _dictStringDup,                /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    _dictStringDestructor,         /* val destructor */
};
#endif

// ******************************************************************************
// ******************************************************************************


// Inernal Include cJSON
// ******************************************************************************
// ******************************************************************************
/* cJSON Types: */
#define cJSON_False 0
#define cJSON_True 1
#define cJSON_NULL 2
#define cJSON_Number 3
#define cJSON_String 4
#define cJSON_Array 5
#define cJSON_Object 6
    
#define cJSON_IsReference 256

/* The cJSON structure: */
typedef struct cJSON {
    struct cJSON *next,*prev;   /* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
    struct cJSON *child;        /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */

    int type;                   /* The type of the item, as above. */

    char *valuestring;          /* The item's string, if type==cJSON_String */
    int valueint;               /* The item's number, if type==cJSON_Number */
    ccint64 valueint64;         /* The item's number, if type==cJSON_Number */
    double valuedouble;         /* The item's number, if type==cJSON_Number */

    char *string;               /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
} cJSON;

typedef struct cJSON_Hooks {
      void *(*malloc_fn)(size_t sz);
      void (*free_fn)(void *ptr);
} cJSON_Hooks;

/* Supply malloc, realloc and free functions to cJSON */
extern void cJSON_InitHooks(cJSON_Hooks* hooks);


/* Supply a block of JSON, and this returns a cJSON object you can interrogate. Call cJSON_Delete when finished. */
extern cJSON *cJSON_Parse(const char *value);
/* Render a cJSON entity to text for transfer/storage. Free the char* when finished. */
extern char  *cJSON_Print(cJSON *item);
/* Render a cJSON entity to text for transfer/storage without any formatting. Free the char* when finished. */
extern char  *cJSON_PrintUnformatted(cJSON *item);
/* Delete a cJSON entity and all subentities. */
extern void   cJSON_Delete(cJSON *c);

/* Returns the number of items in an array (or object). */
extern int    cJSON_GetArraySize(cJSON *array);
/* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
extern cJSON *cJSON_GetArrayItem(cJSON *array,int item);
/* Get item "string" from object. Case insensitive. */
extern cJSON *cJSON_GetObjectItem(cJSON *object,const char *string);

/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when cJSON_Parse() returns 0. 0 when cJSON_Parse() succeeds. */
extern const char *cJSON_GetErrorPtr(void);
    
/* These calls create a cJSON item of the appropriate type. */
extern cJSON *cJSON_CreateNull(void);
extern cJSON *cJSON_CreateTrue(void);
extern cJSON *cJSON_CreateFalse(void);
extern cJSON *cJSON_CreateBool(int b);
extern cJSON *cJSON_CreateNumber(double num);
extern cJSON *cJSON_CreateString(const char *string);
extern cJSON *cJSON_CreateArray(void);
extern cJSON *cJSON_CreateObject(void);

/* These utilities create an Array of count items. */
extern cJSON *cJSON_CreateIntArray(const int *numbers,int count);
extern cJSON *cJSON_CreateFloatArray(const float *numbers,int count);
extern cJSON *cJSON_CreateDoubleArray(const double *numbers,int count);
extern cJSON *cJSON_CreateStringArray(const char **strings,int count);

/* Append item to the specified array/object. */
extern void cJSON_AddItemToArray(cJSON *array, cJSON *item);
extern void cJSON_AddItemToObject(cJSON *object,const char *string,cJSON *item);
/* Append reference to item to the specified array/object. Use this when you want to add an existing cJSON to a new cJSON, but don't want to corrupt your existing cJSON. */
extern void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item);
extern void cJSON_AddItemReferenceToObject(cJSON *object,const char *string,cJSON *item);

/* Remove/Detatch items from Arrays/Objects. */
extern cJSON *cJSON_DetachItemFromArray(cJSON *array,int which);
extern void   cJSON_DeleteItemFromArray(cJSON *array,int which);
extern cJSON *cJSON_DetachItemFromObject(cJSON *object,const char *string);
extern void   cJSON_DeleteItemFromObject(cJSON *object,const char *string);
    
/* Update array items. */
extern void cJSON_ReplaceItemInArray(cJSON *array,int which,cJSON *newitem);
extern void cJSON_ReplaceItemInObject(cJSON *object,const char *string,cJSON *newitem);

/* Duplicate a cJSON item */
extern cJSON *cJSON_Duplicate(cJSON *item,int recurse);
/* Duplicate will create a new, identical cJSON item to the one you pass, in new memory that will
need to be released. With recurse!=0, it will duplicate any children connected to the item.
The item->next and ->prev pointers are always zero on return from Duplicate. */

/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
extern cJSON *cJSON_ParseWithOpts(const char *value,const char **return_parse_end,int require_null_terminated);

extern void cJSON_Minify(char *json);

/* Macros for creating things quickly. */
#define cJSON_AddNullToObject(object,name)      cJSON_AddItemToObject(object, name, cJSON_CreateNull())
#define cJSON_AddTrueToObject(object,name)      cJSON_AddItemToObject(object, name, cJSON_CreateTrue())
#define cJSON_AddFalseToObject(object,name)     cJSON_AddItemToObject(object, name, cJSON_CreateFalse())
#define cJSON_AddBoolToObject(object,name,b)    cJSON_AddItemToObject(object, name, cJSON_CreateBool(b))
#define cJSON_AddNumberToObject(object,name,n)  cJSON_AddItemToObject(object, name, cJSON_CreateNumber(n))
#define cJSON_AddStringToObject(object,name,s)  cJSON_AddItemToObject(object, name, cJSON_CreateString(s))

/* When assigning an integer value, it needs to be propagated to valuedouble too. */
#define cJSON_SetIntValue(object,val)           ((object)?(object)->valueint=(object)->valuedouble=(object)->valueint64=(val):(val))

static const char *ep;

const char *cJSON_GetErrorPtr(void) {return ep;}

static int cJSON_strcasecmp(const char *s1,const char *s2)
{
	if (!s1) return (s1==s2)?0:1;if (!s2) return 1;
	for(; tolower(*s1) == tolower(*s2); ++s1, ++s2)	if(*s1 == 0)	return 0;
	return tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
}

static void *(*cJSON_malloc)(size_t sz) = malloc;
static void (*cJSON_free)(void *ptr) = free;

static char* cJSON_strdup(const char* str)
{
      size_t len;
      char* copy;

      len = strlen(str) + 1;
      if (!(copy = (char*)cJSON_malloc(len))) return 0;
      memcpy(copy,str,len);
      return copy;
}

void cJSON_InitHooks(cJSON_Hooks* hooks)
{
    if (!hooks) { /* Reset hooks */
        cJSON_malloc = malloc;
        cJSON_free = free;
        return;
    }

	cJSON_malloc = (hooks->malloc_fn)?hooks->malloc_fn:malloc;
	cJSON_free	 = (hooks->free_fn)?hooks->free_fn:free;
}

/* Internal constructor. */
static cJSON *cJSON_New_Item(void)
{
	cJSON* node = (cJSON*)cJSON_malloc(sizeof(cJSON));
	if (node) memset(node,0,sizeof(cJSON));
	return node;
}

/* Delete a cJSON structure. */
void cJSON_Delete(cJSON *c)
{
	cJSON *next;
	while (c)
	{
		next=c->next;
		if (!(c->type&cJSON_IsReference) && c->child) cJSON_Delete(c->child);
		if (!(c->type&cJSON_IsReference) && c->valuestring) cJSON_free(c->valuestring);
		if (c->string) cJSON_free(c->string);
		cJSON_free(c);
		c=next;
	}
}

/* Parse the input text to generate a number, and populate the result into item. */
static const char *parse_number(cJSON *item,const char *num)
{
	double n=0,sign=1,scale=0;int subscale=0,signsubscale=1;
    ccint64 n64=0;

	if (*num=='-') sign=-1,num++;	/* Has sign? */
	if (*num=='0') num++;			/* is zero */
	if (*num>='1' && *num<='9')	do	{ n=(n*10.0)+(*num++ -'0'); n64 = (ccint64)n; }	while (*num>='0' && *num<='9');	/* Number? */
	if (*num=='.' && num[1]>='0' && num[1]<='9') {num++;		do	n=(n*10.0)+(*num++ -'0'),scale--; while (*num>='0' && *num<='9');}	/* Fractional part? */
	if (*num=='e' || *num=='E')		/* Exponent? */
	{	num++;if (*num=='+') num++;	else if (*num=='-') signsubscale=-1,num++;		/* With sign? */
		while (*num>='0' && *num<='9') subscale=(subscale*10)+(*num++ - '0');	/* Number? */
	}

	n=sign*n*pow(10.0,(scale+subscale*signsubscale));	/* number = +/- number.fraction * 10^+/- exponent */
    n64=(ccint64)(sign*n64*pow(10, (subscale*signsubscale)));
	
	item->valuedouble=n;
	item->valueint=(int)n;
    item->valueint64=n64;
	item->type=cJSON_Number;
	return num;
}

/* Render the number nicely from the given item into a string. */
static char *print_number(cJSON *item)
{
	char *str;
	double d=item->valuedouble;
	if (fabs(((double)item->valueint)-d)<=DBL_EPSILON && d<=INT_MAX && d>=INT_MIN)
	{
		str=(char*)cJSON_malloc(21);	/* 2^64+1 can be represented in 21 chars. */
		if (str) sprintf(str, "%d",item->valueint);
	} else if (fabs(((double)item->valueint)-d)<=DBL_EPSILON && d>=INT_MAX) 
    {
        str=(char*)cJSON_malloc(21);	/* 2^64+1 can be represented in 21 chars. */
        if (str) sprintf(str, "%lld",item->valueint64);
    }
	else
	{
		str=(char*)cJSON_malloc(64);	/* This is a nice tradeoff. */
		if (str)
		{
			if (fabs(floor(d)-d)<=DBL_EPSILON && fabs(d)<1.0e60)sprintf(str, "%.0f",d);
			else if (fabs(d)<1.0e-6 || fabs(d)>1.0e9)			sprintf(str, "%e",d);
			else												sprintf(str, "%f",d);
		}
	}
	return str;
}

static unsigned parse_hex4(const char *str)
{
	unsigned h=0;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	h=h<<4;str++;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	h=h<<4;str++;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	h=h<<4;str++;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	return h;
}

/* Parse the input text into an unescaped cstring, and populate item. */
static const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const char *parse_string(cJSON *item,const char *str)
{
	const char *ptr=str+1;char *ptr2;char *out;int len=0;unsigned uc,uc2;
	if (*str!='\"') {ep=str;return 0;}	/* not a string! */
	
	while (*ptr!='\"' && *ptr && ++len) if (*ptr++ == '\\') ptr++;	/* Skip escaped quotes. */
	
	out=(char*)cJSON_malloc(len+1);	/* This is how long we need for the string, roughly. */
	if (!out) return 0;
	
	ptr=str+1;ptr2=out;
	while (*ptr!='\"' && *ptr)
	{
		if (*ptr!='\\') *ptr2++=*ptr++;
		else
		{
			ptr++;
			switch (*ptr)
			{
				case 'b': *ptr2++='\b';	break;
				case 'f': *ptr2++='\f';	break;
				case 'n': *ptr2++='\n';	break;
				case 'r': *ptr2++='\r';	break;
				case 't': *ptr2++='\t';	break;
				case 'u':	 /* transcode utf16 to utf8. */
					uc=parse_hex4(ptr+1);ptr+=4;	/* get the unicode char. */

					if ((uc>=0xDC00 && uc<=0xDFFF) || uc==0)	break;	/* check for invalid.	*/

					if (uc>=0xD800 && uc<=0xDBFF)	/* UTF16 surrogate pairs.	*/
					{
						if (ptr[1]!='\\' || ptr[2]!='u')	break;	/* missing second-half of surrogate.	*/
						uc2=parse_hex4(ptr+3);ptr+=6;
						if (uc2<0xDC00 || uc2>0xDFFF)		break;	/* invalid second-half of surrogate.	*/
						uc=0x10000 + (((uc&0x3FF)<<10) | (uc2&0x3FF));
					}

					len=4;if (uc<0x80) len=1;else if (uc<0x800) len=2;else if (uc<0x10000) len=3; ptr2+=len;
					
					switch (len) {
						case 4: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 3: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 2: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 1: *--ptr2 =(uc | firstByteMark[len]);
					}
					ptr2+=len;
					break;
				default:  *ptr2++=*ptr; break;
			}
			ptr++;
		}
	}
	*ptr2=0;
	if (*ptr=='\"') ptr++;
	item->valuestring=out;
	item->type=cJSON_String;
	return ptr;
}

/* Render the cstring provided to an escaped version that can be printed. */
static char *print_string_ptr(const char *str)
{
	const char *ptr;char *ptr2,*out;int len=0;unsigned char token;
	
	if (!str) return cJSON_strdup("");
	ptr=str;while ((token=*ptr) && ++len) {if (strchr("\"\\\b\f\n\r\t",token)) len++; else if (token<32) len+=5;ptr++;}
	
	out=(char*)cJSON_malloc(len+3);
	if (!out) return 0;

	ptr2=out;ptr=str;
	*ptr2++='\"';
	while (*ptr)
	{
		if ((unsigned char)*ptr>31 && *ptr!='\"' && *ptr!='\\') *ptr2++=*ptr++;
		else
		{
			*ptr2++='\\';
			switch (token=*ptr++)
			{
				case '\\':	*ptr2++='\\';	break;
				case '\"':	*ptr2++='\"';	break;
				case '\b':	*ptr2++='b';	break;
				case '\f':	*ptr2++='f';	break;
				case '\n':	*ptr2++='n';	break;
				case '\r':	*ptr2++='r';	break;
				case '\t':	*ptr2++='t';	break;
				default: sprintf(ptr2, "u%04x",token);ptr2+=5;	break;	/* escape and print */
			}
		}
	}
	*ptr2++='\"';*ptr2++=0;
	return out;
}
/* Invote print_string_ptr (which is useful) on an item. */
static char *print_string(cJSON *item)	{return print_string_ptr(item->valuestring);}

/* Predeclare these prototypes. */
static const char *parse_value(cJSON *item,const char *value);
static char *print_value(cJSON *item,int depth,int fmt);
static const char *parse_array(cJSON *item,const char *value);
static char *print_array(cJSON *item,int depth,int fmt);
static const char *parse_object(cJSON *item,const char *value);
static char *print_object(cJSON *item,int depth,int fmt);

/* Utility to jump whitespace and cr/lf */
static const char *skip(const char *in) {while (in && *in && (unsigned char)*in<=32) in++; return in;}

/* Parse an object - create a new root, and populate. */
cJSON *cJSON_ParseWithOpts(const char *value,const char **return_parse_end,int require_null_terminated)
{
	const char *end=0;
	cJSON *c=cJSON_New_Item();
	ep=0;
	if (!c) return 0;       /* memory fail */

	end=parse_value(c,skip(value));
	if (!end)	{cJSON_Delete(c);return 0;}	/* parse failure. ep is set. */

	/* if we require null-terminated JSON without appended garbage, skip and then check for a null terminator */
	if (require_null_terminated) {end=skip(end);if (*end) {cJSON_Delete(c);ep=end;return 0;}}
	if (return_parse_end) *return_parse_end=end;
	return c;
}
/* Default options for cJSON_Parse */
cJSON *cJSON_Parse(const char *value) {return cJSON_ParseWithOpts(value,0,0);}

/* Render a cJSON item/entity/structure to text. */
char *cJSON_Print(cJSON *item)				{return print_value(item,0,1);}
char *cJSON_PrintUnformatted(cJSON *item)	{return print_value(item,0,0);}

/* Parser core - when encountering text, process appropriately. */
static const char *parse_value(cJSON *item,const char *value)
{
	if (!value)						return 0;	/* Fail on null. */
	if (!strncmp(value,"null",4))	{ item->type=cJSON_NULL;  return value+4; }
	if (!strncmp(value,"false",5))	{ item->type=cJSON_False; return value+5; }
	if (!strncmp(value,"true",4))	{ item->type=cJSON_True; item->valueint=1;	return value+4; }
	if (*value=='\"')				{ return parse_string(item,value); }
	if (*value=='-' || (*value>='0' && *value<='9'))	{ return parse_number(item,value); }
	if (*value=='[')				{ return parse_array(item,value); }
	if (*value=='{')				{ return parse_object(item,value); }

	ep=value;return 0;	/* failure. */
}

/* Render a value to text. */
static char *print_value(cJSON *item,int depth,int fmt)
{
	char *out=0;
	if (!item) return 0;
	switch ((item->type)&255)
	{
		case cJSON_NULL:	out=cJSON_strdup("null");	break;
		case cJSON_False:	out=cJSON_strdup("false");break;
		case cJSON_True:	out=cJSON_strdup("true"); break;
		case cJSON_Number:	out=print_number(item);break;
		case cJSON_String:	out=print_string(item);break;
		case cJSON_Array:	out=print_array(item,depth,fmt);break;
		case cJSON_Object:	out=print_object(item,depth,fmt);break;
	}
	return out;
}

/* Build an array from input text. */
static const char *parse_array(cJSON *item,const char *value)
{
	cJSON *child;
	if (*value!='[')	{ep=value;return 0;}	/* not an array! */

	item->type=cJSON_Array;
	value=skip(value+1);
	if (*value==']') return value+1;	/* empty array. */

	item->child=child=cJSON_New_Item();
	if (!item->child) return 0;		 /* memory fail */
	value=skip(parse_value(child,skip(value)));	/* skip any spacing, get the value. */
	if (!value) return 0;

	while (*value==',')
	{
		cJSON *new_item;
		if (!(new_item=cJSON_New_Item())) return 0; 	/* memory fail */
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_value(child,skip(value+1)));
		if (!value) return 0;	/* memory fail */
	}

	if (*value==']') return value+1;	/* end of array */
	ep=value;return 0;	/* malformed. */
}

/* Render an array to text */
static char *print_array(cJSON *item,int depth,int fmt)
{
	char **entries;
	char *out=0,*ptr,*ret;int len=5;
	cJSON *child=item->child;
	int numentries=0,i=0,fail=0;
	
	/* How many entries in the array? */
	while (child) numentries++,child=child->next;
	/* Explicitly handle numentries==0 */
	if (!numentries)
	{
		out=(char*)cJSON_malloc(3);
		if (out) strcpy(out,"[]");
		return out;
	}
	/* Allocate an array to hold the values for each */
	entries=(char**)cJSON_malloc(numentries*sizeof(char*));
	if (!entries) return 0;
	memset(entries,0,numentries*sizeof(char*));
	/* Retrieve all the results: */
	child=item->child;
	while (child && !fail)
	{
		ret=print_value(child,depth+1,fmt);
		entries[i++]=ret;
		if (ret) len+=strlen(ret)+2+(fmt?1:0); else fail=1;
		child=child->next;
	}
	
	/* If we didn't fail, try to malloc the output string */
	if (!fail) out=(char*)cJSON_malloc(len);
	/* If that fails, we fail. */
	if (!out) fail=1;

	/* Handle failure. */
	if (fail)
	{
		for (i=0;i<numentries;i++) if (entries[i]) cJSON_free(entries[i]);
		cJSON_free(entries);
		return 0;
	}
	
	/* Compose the output array. */
	*out='[';
	ptr=out+1;*ptr=0;
	for (i=0;i<numentries;i++)
	{
		strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
		if (i!=numentries-1) {*ptr++=',';if(fmt)*ptr++=' ';*ptr=0;}
		cJSON_free(entries[i]);
	}
	cJSON_free(entries);
	*ptr++=']';*ptr++=0;
	return out;	
}

/* Build an object from the text. */
static const char *parse_object(cJSON *item,const char *value)
{
	cJSON *child;
	if (*value!='{')	{ep=value;return 0;}	/* not an object! */
	
	item->type=cJSON_Object;
	value=skip(value+1);
	if (*value=='}') return value+1;	/* empty array. */
	
	item->child=child=cJSON_New_Item();
	if (!item->child) return 0;
	value=skip(parse_string(child,skip(value)));
	if (!value) return 0;
	child->string=child->valuestring;child->valuestring=0;
	if (*value!=':') {ep=value;return 0;}	/* fail! */
	value=skip(parse_value(child,skip(value+1)));	/* skip any spacing, get the value. */
	if (!value) return 0;
	
	while (*value==',')
	{
		cJSON *new_item;
		if (!(new_item=cJSON_New_Item()))	return 0; /* memory fail */
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_string(child,skip(value+1)));
		if (!value) return 0;
		child->string=child->valuestring;child->valuestring=0;
		if (*value!=':') {ep=value;return 0;}	/* fail! */
		value=skip(parse_value(child,skip(value+1)));	/* skip any spacing, get the value. */
		if (!value) return 0;
	}
	
	if (*value=='}') return value+1;	/* end of array */
	ep=value;return 0;	/* malformed. */
}

/* Render an object to text. */
static char *print_object(cJSON *item,int depth,int fmt)
{
	char **entries=0,**names=0;
	char *out=0,*ptr,*ret,*str;int len=7,i=0,j;
	cJSON *child=item->child;
	int numentries=0,fail=0;
	/* Count the number of entries. */
	while (child) numentries++,child=child->next;
	/* Explicitly handle empty object case */
	if (!numentries)
	{
		out=(char*)cJSON_malloc(fmt?depth+4:3);
		if (!out)	return 0;
		ptr=out;*ptr++='{';
		if (fmt) {*ptr++='\n';for (i=0;i<depth-1;i++) *ptr++='\t';}
		*ptr++='}';*ptr++=0;
		return out;
	}
	/* Allocate space for the names and the objects */
	entries=(char**)cJSON_malloc(numentries*sizeof(char*));
	if (!entries) return 0;
	names=(char**)cJSON_malloc(numentries*sizeof(char*));
	if (!names) {cJSON_free(entries);return 0;}
	memset(entries,0,sizeof(char*)*numentries);
	memset(names,0,sizeof(char*)*numentries);

	/* Collect all the results into our arrays: */
	child=item->child;depth++;if (fmt) len+=depth;
	while (child)
	{
		names[i]=str=print_string_ptr(child->string);
		entries[i++]=ret=print_value(child,depth,fmt);
		if (str && ret) len+=strlen(ret)+strlen(str)+2+(fmt?2+depth:0); else fail=1;
		child=child->next;
	}
	
	/* Try to allocate the output string */
	if (!fail) out=(char*)cJSON_malloc(len);
	if (!out) fail=1;

	/* Handle failure */
	if (fail)
	{
		for (i=0;i<numentries;i++) {if (names[i]) cJSON_free(names[i]);if (entries[i]) cJSON_free(entries[i]);}
		cJSON_free(names);cJSON_free(entries);
		return 0;
	}
	
	/* Compose the output: */
	*out='{';ptr=out+1;if (fmt)*ptr++='\n';*ptr=0;
	for (i=0;i<numentries;i++)
	{
		if (fmt) for (j=0;j<depth;j++) *ptr++='\t';
		strcpy(ptr,names[i]);ptr+=strlen(names[i]);
		*ptr++=':';if (fmt) *ptr++='\t';
		strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
		if (i!=numentries-1) *ptr++=',';
		if (fmt) *ptr++='\n';*ptr=0;
		cJSON_free(names[i]);cJSON_free(entries[i]);
	}
	
	cJSON_free(names);cJSON_free(entries);
	if (fmt) for (i=0;i<depth-1;i++) *ptr++='\t';
	*ptr++='}';*ptr++=0;
	return out;	
}

/* Get Array size/item / object item. */
int    cJSON_GetArraySize(cJSON *array)							{cJSON *c=array->child;int i=0;while(c)i++,c=c->next;return i;}
cJSON *cJSON_GetArrayItem(cJSON *array,int item)				{cJSON *c=array->child;  while (c && item>0) item--,c=c->next; return c;}
cJSON *cJSON_GetObjectItem(cJSON *object,const char *string)	{cJSON *c=object->child; while (c && cJSON_strcasecmp(c->string,string)) c=c->next; return c;}

/* Utility for array list handling. */
static void suffix_object(cJSON *prev,cJSON *item) {prev->next=item;item->prev=prev;}
/* Utility for handling references. */
static cJSON *create_reference(cJSON *item) {cJSON *ref=cJSON_New_Item();if (!ref) return 0;memcpy(ref,item,sizeof(cJSON));ref->string=0;ref->type|=cJSON_IsReference;ref->next=ref->prev=0;return ref;}

/* Add item to array/object. */
void   cJSON_AddItemToArray(cJSON *array, cJSON *item)						{cJSON *c=array->child;if (!item) return; if (!c) {array->child=item;} else {while (c && c->next) c=c->next; suffix_object(c,item);}}
void   cJSON_AddItemToObject(cJSON *object,const char *string,cJSON *item)	{if (!item) return; if (item->string) cJSON_free(item->string);item->string=cJSON_strdup(string);cJSON_AddItemToArray(object,item);}
void	cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item)						{cJSON_AddItemToArray(array,create_reference(item));}
void	cJSON_AddItemReferenceToObject(cJSON *object,const char *string,cJSON *item)	{cJSON_AddItemToObject(object,string,create_reference(item));}

cJSON *cJSON_DetachItemFromArray(cJSON *array,int which)			{cJSON *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return 0;
	if (c->prev) c->prev->next=c->next;if (c->next) c->next->prev=c->prev;if (c==array->child) array->child=c->next;c->prev=c->next=0;return c;}
void   cJSON_DeleteItemFromArray(cJSON *array,int which)			{cJSON_Delete(cJSON_DetachItemFromArray(array,which));}
cJSON *cJSON_DetachItemFromObject(cJSON *object,const char *string) {int i=0;cJSON *c=object->child;while (c && cJSON_strcasecmp(c->string,string)) i++,c=c->next;if (c) return cJSON_DetachItemFromArray(object,i);return 0;}
void   cJSON_DeleteItemFromObject(cJSON *object,const char *string) {cJSON_Delete(cJSON_DetachItemFromObject(object,string));}

/* Replace array/object items with new ones. */
void   cJSON_ReplaceItemInArray(cJSON *array,int which,cJSON *newitem)		{cJSON *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return;
	newitem->next=c->next;newitem->prev=c->prev;if (newitem->next) newitem->next->prev=newitem;
	if (c==array->child) array->child=newitem; else newitem->prev->next=newitem;c->next=c->prev=0;cJSON_Delete(c);}
void   cJSON_ReplaceItemInObject(cJSON *object,const char *string,cJSON *newitem){int i=0;cJSON *c=object->child;while(c && cJSON_strcasecmp(c->string,string))i++,c=c->next;if(c){newitem->string=cJSON_strdup(string);cJSON_ReplaceItemInArray(object,i,newitem);}}

/* Create basic types: */
cJSON *cJSON_CreateNull(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_NULL;return item;}
cJSON *cJSON_CreateTrue(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_True;return item;}
cJSON *cJSON_CreateFalse(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_False;return item;}
cJSON *cJSON_CreateBool(int b)					{cJSON *item=cJSON_New_Item();if(item)item->type=b?cJSON_True:cJSON_False;return item;}
cJSON *cJSON_CreateNumber(double num)			{cJSON *item=cJSON_New_Item();if(item){item->type=cJSON_Number;item->valuedouble=num;item->valueint=(int)num;}return item;}
cJSON *cJSON_CreateNumber64(ccint64 num)			{cJSON *item=cJSON_New_Item();if(item){item->type=cJSON_Number;item->valueint64 = num; item->valuedouble=(double)num;item->valueint=(int)num;}return item;}
cJSON *cJSON_CreateString(const char *string)	{cJSON *item=cJSON_New_Item();if(item){item->type=cJSON_String;item->valuestring=cJSON_strdup(string);}return item;}
cJSON *cJSON_CreateArray(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_Array;return item;}
cJSON *cJSON_CreateObject(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_Object;return item;}

/* Create Arrays: */
cJSON *cJSON_CreateIntArray(const int *numbers,int count)		{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateFloatArray(const float *numbers,int count)	{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateDoubleArray(const double *numbers,int count)	{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateStringArray(const char **strings,int count)	{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateString(strings[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}

/* Duplication */
cJSON *cJSON_Duplicate(cJSON *item,int recurse)
{
	cJSON *newitem,*cptr,*nptr=0,*newchild;
	/* Bail on bad ptr */
	if (!item) return 0;
	/* Create new item */
	newitem=cJSON_New_Item();
	if (!newitem) return 0;
	/* Copy over all vars */
	newitem->type=item->type&(~cJSON_IsReference),newitem->valueint=item->valueint,newitem->valuedouble=item->valuedouble;newitem->valueint64=item->valueint64;
	if (item->valuestring)	{newitem->valuestring=cJSON_strdup(item->valuestring);	if (!newitem->valuestring)	{cJSON_Delete(newitem);return 0;}}
	if (item->string)		{newitem->string=cJSON_strdup(item->string);			if (!newitem->string)		{cJSON_Delete(newitem);return 0;}}
	/* If non-recursive, then we're done! */
	if (!recurse) return newitem;
	/* Walk the ->next chain for the child. */
	cptr=item->child;
	while (cptr)
	{
		newchild=cJSON_Duplicate(cptr,1);		/* Duplicate (with recurse) each item in the ->next chain */
		if (!newchild) {cJSON_Delete(newitem);return 0;}
		if (nptr)	{nptr->next=newchild,newchild->prev=nptr;nptr=newchild;}	/* If newitem->child already set, then crosswire ->prev and ->next and move on */
		else		{newitem->child=newchild;nptr=newchild;}					/* Set newitem->child and move to it */
		cptr=cptr->next;
	}
	return newitem;
}

void cJSON_Minify(char *json)
{
	char *into=json;
	while (*json)
	{
		if (*json==' ') json++;
		else if (*json=='\t') json++;	/* Whitespace characters. */
		else if (*json=='\r') json++;
		else if (*json=='\n') json++;
		else if (*json=='/' && json[1]=='/')  while (*json && *json!='\n') json++;	/* double-slash comments, to end of line. */
		else if (*json=='/' && json[1]=='*') {while (*json && !(*json=='*' && json[1]=='/')) json++;json+=2;}	/* multiline comments. */
		else if (*json=='\"'){*into++=*json++;while (*json && *json!='\"'){if (*json=='\\') *into++=*json++;*into++=*json++;}*into++=*json++;} /* string literals, which are \" sensitive. */
		else *into++=*json++;			/* All other characters. */
	}
	*into=0;	/* and null-terminate. */
}
// ******************************************************************************
// ******************************************************************************

#define cccheck(exp) do { if(!(exp)) { return ; } } while(0)
#define cccheckret(exp, ret) do { if(!(exp)) { return ret; }} while(0)
#define __ccmalloc(type) (type*)cc_alloc(sizeof(type));

#ifdef WIN32
int
gettimeofday(struct timeval *tp, void *tzp)
{
    time_t clock;
    struct tm tm;
    SYSTEMTIME wtm;
    cc_unused(tzp);

    GetLocalTime(&wtm);
    tm.tm_year     = wtm.wYear - 1900;
    tm.tm_mon     = wtm.wMonth - 1;
    tm.tm_mday     = wtm.wDay;
    tm.tm_hour     = wtm.wHour;
    tm.tm_min     = wtm.wMinute;
    tm.tm_sec     = wtm.wSecond;
    tm.tm_isdst    = -1;
    clock = mktime(&tm);
    tp->tv_sec = (long)clock;
    tp->tv_usec = (long)wtm.wMilliseconds * 1000;
    return (0);
}
#endif

// get current system time in nanos 
ccint64 ccgetcurnano() {
    struct timeval tv;
    
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000*1000 + tv.tv_usec;
}

// get current system time in millseconds
ccint64 ccgetcurtick() {
    return ccgetcurnano()/1000;
}

// alawys get the next nano of current
ccint64 ccgetnextnano(){
    static ccint64 gseq = 0;
    ccint64 curseq = ccgetcurnano();
    if (curseq > gseq) {
        gseq = curseq;
    }else {
        ++gseq;
    }
    return gseq;
}

// memory tag: will set in basic json object header
#define __CC_JSON_OBJ  1 
#define __CC_JSON_ARRAY 1<<1

// ******************************************************************************
// basic memory system object
typedef struct __cc_content {
    size_t flag;
    size_t size;
    struct __cc_content *next;
    struct __cc_content *pre;

    char content[];
}__cc_content;

// inherti the basic system object : like the array
#define __internal_cc_content size_t flag; size_t size; struct __cc_content *next; struct __cc_content *pre; char content[]

// make right basic memory system object pointer
#define __to_content(p) (__cc_content*)((char*)p - sizeof(__cc_content))

// statics about the memory state
static size_t gmemalloc = 0;
static size_t gmemfree = 0;
static size_t gmemexpand = sizeof(__cc_content) + 1;

// keep the thred safe for memory system
#ifdef WIN32 
static HANDLE _g_mem_mutex_get() {
static HANDLE gmutex = 0;
if (gmutex == 0) {
    gmutex = CreateMutex(NULL, 0, NULL);
}
return gmutex;
}
#define __ccmemlock WaitForSingleObject(_g_mem_mutex_get(), 0)
#define __ccmemunlock ReleaseMutex(_g_mem_mutex_get())
#else
static pthread_mutex_t *_g_mem_mutex_get() {
static pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER; 
return &mutex;
}
#define __ccmemlock pthread_mutex_lock(_g_mem_mutex_get())
#define __ccmemunlock pthread_mutex_unlock(_g_mem_mutex_get()) 
#endif

// print the memory states
size_t cc_mem_state() {
    size_t hold = 0;
    __ccmemlock;
    hold = gmemalloc-gmemfree; 
    printf("[CCJSON-Memory] malloc: %ld, free: %ld, hold: %ld\n", 
            gmemalloc, 
            gmemfree, 
            hold);
    __ccmemunlock;

    return hold;
} 

// get current memory useage total size
size_t cc_mem_size() {
    size_t hold = 0;

    __ccmemlock;
    hold = gmemalloc-gmemfree; 
    __ccmemunlock;

    return hold;
}

// memory alloc
static char *__cc_alloc(size_t size) {
    __cc_content *content = (__cc_content*)calloc(1, size + gmemexpand);
    content->size = size;

    __ccmemlock;
    gmemalloc += (size + gmemexpand);
    __ccmemunlock;

    return content->content;
}

// memory free
static void __cc_free(char *c) {
    __cc_content *content;
    cccheck(c);
    content = (__cc_content*)(c - sizeof(__cc_content));

    __ccmemlock;
    gmemfree += (content->size + gmemexpand); 
    __ccmemunlock;

    free(content);
}

// memory len 
static size_t __cc_len(char *c) {
    __cc_content *content;
    cccheckret(c, 0);
    content = (__cc_content*)(c - sizeof(__cc_content));
    return content->size;
}

// set the memory object flag
#define __cc_setflag(p, xflag)  do { \
    __cc_content *content = __to_content(p); \
    if (content) { content->flag |= xflag; } \
    } while(0) 

// judge the memory flag
#define __cc_hasflag(p, xflag) (p && ((__to_content(p))->flag & xflag))

// clear the memory flag
#define __cc_unsetflag(p, xflag)  do { \
    __cc_content *content = __to_content(p); \
    if (content) { content->flag &= ~xflag; } \
    } while(0) 

// basic memory cache start size
static const size_t __cc_isize = 4;
// basic memory cache layer 
struct {
    size_t size;
    size_t capacity;
    size_t len;
    __cc_content *base;
}gmemcache[14]; // 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768
// basic memory cache count
static const int gmemcachecount = sizeof(gmemcache)/sizeof(gmemcache[0]);

// init the memory cache
static void __ccinitmemcache() {
    static int init = 0;
    int i;
    if (init) {
        return;
    }
    ++init;
    __ccmemlock;
    for (i=0; i<gmemcachecount; ++i) {
        gmemcache[i].size = __cc_isize<<i;
        gmemcache[i].capacity = 100;
        gmemcache[i].len = 0;
        gmemcache[i].base = 0;
    }
    __ccmemunlock;
}

// print the memory state
static size_t __ccmemcachestate() {
    size_t memsize = 0;
    int i;

    // init
    __ccinitmemcache();
    // print memory cache
    printf("[CCJSON-Memory-Cache] count: %d\n", 
            gmemcachecount);
    __ccmemlock;
    for (i=0; i<gmemcachecount; ++i) {
        printf("[CCJSON-Memory-Cache] ID: %d, "
                "chuck size: %ld, "
                "capacity: %ld, " 
                "current: %ld, "
                "hold: %ld \n",
                i, 
                gmemcache[i].size,
                gmemcache[i].capacity,
                gmemcache[i].len,
                gmemcache[i].len * gmemcache[i].size);
        memsize += gmemcache[i].len * gmemcache[i].size; 
    }
    __ccmemunlock;
    return memsize;
}

// print the memory states, and return the memory size hold by caches
size_t cc_mem_cache_state() {
    return __ccmemcachestate();
}

// clear the memory cache of index
void cc_mem_cache_clearof(int index) {
    // init the memory cache
    __ccinitmemcache();
    // check if the index is illege 
    cccheck(index>=0 && index<gmemcachecount);

    // free all memory cache with index
    __ccmemlock; 
    while(gmemcache[index].base) {
        __cc_content* cache = gmemcache[index].base; 
        gmemcache[index].base = cache->next;
        // 释放内存
        __cc_free(cache->content);
    }
    gmemcache[index].len = 0;
    gmemcache[index].base = NULL;
    __ccmemunlock;
}

// clear all memory cache
void cc_mem_cache_clear() {
    int i;
    for (i=0; i<gmemcachecount; ++i) {
        cc_mem_cache_clearof(i);
    }
}

// get the memory cache unit size by index 
size_t cc_mem_cache_current(int index) {
    cccheckret(index>=0 && index<gmemcachecount, 0);
    return gmemcache[index].len;
}

// get the memory cache capacity by index
size_t cc_mem_cache_capacity(int index) {
    cccheckret(index>=0 && index<gmemcachecount, 0);
    return gmemcache[index].capacity; 
}

// set the memory capacity, set the capacity to 0 will disable the cache
void cc_mem_cache_setcapacity(int index, size_t capacity) {
    cccheck(index>=0 && index<gmemcachecount);
    __ccmemlock;
    gmemcache[index].capacity = capacity;
    __ccmemunlock;
}

// 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
static int __ccsizeindex(size_t size) {
    int index = 0;
    cccheckret(size, 0);
    size = (size-1)/__cc_isize;
    while(size) {
        ++index;
        size = size >> 1;
    }
    return index;
}

// get a basic memory object from memory cache
static char* cc_mem_alloc(size_t size) {
    int index;
    __cc_content* cache;
    char* mem;

    // init the memory cache 
    __ccinitmemcache();
    // caculate the memory cache index
    index = __ccsizeindex(size);
    if (index < 0 || index >gmemcachecount) {
        return __cc_alloc(size);
    }
    // if the memory cache have something
    __ccmemlock; 
    if (gmemcache[index].base) {
        cache = gmemcache[index].base; 

        gmemcache[index].base = cache->next;
        --gmemcache[index].len;

        if (cache->next) {
            cache->next->pre = NULL;
            cache->next = NULL;
        }
        // clear memory
        memset(cache->content, 0, size+1);
        // clear flag
        cache->flag = 0;

        // return
        mem = cache->content;
    } else {
        // the cache is empty, get one from the heap 
        mem = __cc_alloc(gmemcache[index].size);
        mem[size] = 0;
    }
    __ccmemunlock;

    return mem;
}

static void cc_mem_free(char *c) {
    __cc_content *content;
    int index;

    cccheck(c);
    // init memory cache 
    __ccinitmemcache();
    // get the basic memery object pointer
    content = (__cc_content*)(c - sizeof(__cc_content));
    index = __ccsizeindex(content->size);
    if (index < 0 || index >gmemcachecount) {
        __cc_free(c);
        return;
    }
    // free basic memory object to cache
    __ccmemlock; 
    if (gmemcache[index].len < gmemcache[index].capacity) {
        content->next = gmemcache[index].base;
        if (content->next) {
            content->next->pre = content;
        }
        content->pre = NULL;
        content->flag = 0;

        gmemcache[index].base = content;
        ++gmemcache[index].len;
    } else {
        // the cache is full, juse return to heap
        __cc_free(c);
    }
    __ccmemunlock;
}

// the shuffter about the memory cache system
ccibool ccenablememcache = cciyes;

// enable and disable the memory cache system 
ccibool cc_enablememorycache(ccibool enable) {
    ccibool ret = cciyes; 
    __ccmemlock;
    if (ccenablememcache != enable) {
        ccenablememcache = enable;
        __ccmemunlock;
        ret = !ccenablememcache;
    } else {
        ret =  ccenablememcache;
    }
    __ccmemunlock;
    return ret;
}

// memory alloc, all memory will end with 0,  call cc_free to free
char *cc_alloc(size_t size) {
    if (ccenablememcache) {
        return cc_mem_alloc(size);
    }else {
        return __cc_alloc(size);
    }
}

// free memory from cc_alloc
void cc_free(char *c) {
    if (ccenablememcache) {
        cc_mem_free(c);
    }else {
        __cc_free(c);
    }
}

// get the memory size(bytes)
size_t cc_len(char *c) {
    return __cc_len(c);
}

// make copy of string, and free memory with cc_free
char *cc_dup(const char* src) {
    size_t len;
    char* content;

    cccheckret(src, NULL);
    len = strlen(src);
    content = cc_alloc(len);
    memcpy(content, src, len);
    return content;
}

// ******************************************************************************
// return the all content of file, need free with cc_free
char * cc_read_file(const char* fn) {
    char *content = NULL;
    size_t filesize = 0;
    FILE *file = 0;

    file = fopen(fn, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        filesize = ftell(file);
        rewind(file);

        content = cc_alloc(filesize);
        fread(content, 1, filesize, file);
        fclose(file);
    }
    return content;
}

// write content to file, content must returned from cc_alloc, cc_dup, cc_read_file
size_t cc_write_file(char* content, const char* fn) {
    size_t write = 0;
    FILE *file = fopen(fn, "w");
    if (file) {
        write = fwrite(content, 1, cc_len(content), file);
        fclose(file);
    }
    return write;
}

// ******************************************************************************
// self define the dict
/* ----------------------- StringCopy Hash Table Type ------------------------*/

static unsigned int _dictStringCopyHTHashFunction(const void *key)
{
    return dictGenHashFunction(key, (int)strlen((char*)key));
}

static void *_dictStringDup(void *privdata, const void *key)
{
    size_t len = strlen((char*)key);
    char *copy = (char*)malloc(len+1);
    DICT_NOTUSED(privdata);
    
    memcpy(copy, key, len);
    copy[len] = '\0';
    return copy;
}

static int _dictStringCopyHTKeyCompare(void *privdata, const void *key1,
                                       const void *key2)
{
    const char* key1char = (const char*)key1;
    const char* key2char = (const char*)key2;
    DICT_NOTUSED(privdata);

    return strcmp(key1char, key2char) == 0;
}

static void _dictStringDestructor(void *privdata, void *key)
{
    DICT_NOTUSED(privdata);
    
    free(key);
}

static dictType xdictTypeHeapStringCopyKey = {
    _dictStringCopyHTHashFunction, /* hash function */
    _dictStringDup,                /* key dup */
    NULL,                          /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    NULL                           /* val destructor */
};

#ifdef WIN32
// disable the warning about use the extend of zero size array in struct
#pragma warning(disable: 4200)
#endif

// ******************************************************************************
// basic json object
typedef struct ccjson_obj {
    int __index;
    int __flag;
    char __has[];
}ccjson_obj;

// basic information: has
char ccjsonobjhas(void *obj, int index) {
    ccjson_obj *iobj = (ccjson_obj*)obj;
    return iobj->__has[index]; 
}

// basic information: index
int ccjsonobjindex(void *obj) {
    ccjson_obj *iobj = (ccjson_obj*)obj;
    return iobj->__index;
}

// basic information: flag
int ccjsonobjflag(void *obj) {
    ccjson_obj *iobj = (ccjson_obj*)obj;
    return iobj->__flag;
}

// ******************************************************************************
// array (maybe we will add some salts in struct)
typedef struct ccjsonarray {
    size_t n;
    size_t nsize;
    ccjson_obj *obj0;
    ccjson_obj *obj1; // bytes aligment 

    __internal_cc_content;
}ccjsonarray;

// array element basic json object 
static ccjson_obj *_ccjsonobjallocdynamic(int index, size_t n) {
    ccjson_obj *obj = (ccjson_obj*)cc_alloc( sizeof(ccjson_obj) + 2*(n + 7)/8);
    obj->__index = index;
    return obj;
}

/**
 */
void * ccarraymalloc(size_t n, size_t size, int index) {
    ccjsonarray * p = (ccjsonarray*)cc_alloc(n * size + sizeof(ccjsonarray));
    p->n = n;
    p->nsize = size;
    p->obj0 = _ccjsonobjallocdynamic(index, n);
    __cc_setflag(p->content, __CC_JSON_ARRAY);
    return p->content;
}

/**
 * */
void * ccarraymallocof(size_t n, cctypemeta* meta) {
    return ccarraymalloc(n, meta->size, meta->index);
}

/**
 * */
cctypemeta* ccarraymeta(void *array) {
    ccjsonarray * p;
    cccheckret(array, NULL);
    p = (ccjsonarray*)((char*)array - sizeof(ccjsonarray));
    return ccgettypemetaof(p->obj0->__index);
}

/**
 * get the basic json object about array, 
 * then we will do same thing in array as in json object 
 * */
ccjson_obj* ccarrayobj(void *array) {
    ccjsonarray * p;
    cccheckret(array, NULL);
    p = (ccjsonarray*)((char*)array - sizeof(ccjsonarray));
    return p->obj0;
}


/**
 */
void ccarrayfree(void *array) {
    ccjsonarray * p;
    cccheck(array);
    p = (ccjsonarray*)((char*)array - sizeof(ccjsonarray));
    cc_free((char*)p->obj0);
    cc_free((char*)p);
}

/**
 */
size_t ccarraylen(void *array) {
    ccjsonarray *p;
    cccheckret(array, 0);
    p = (ccjsonarray*)((char*)array - sizeof(ccjsonarray));
    return p->n;
}

// ******************************************************************************
// find meta by name {name : parse_value};
static dict *gparses = NULL;
static struct cctypemeta *gtypemetas[CCMaxTypeCount];
static int gtypemetascnt = 0;

// keep thread safe for meta system
#ifdef WIN32
static HANDLE _g_meta_mutex_get() {
static HANDLE gmutex = 0;
if (gmutex == 0) {
    gmutex = CreateMutex(NULL, 0, NULL);
}
return gmutex;
}
#define __ccmetalock WaitForSingleObject(_g_meta_mutex_get(), 0)
#define __ccmetaunlock ReleaseMutex(_g_meta_mutex_get())
#else
static pthread_mutex_t *_g_meta_mutex_get() {
static pthread_mutex_t gmetamutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
return &gmetamutex;
}
#define __ccmetalock pthread_mutex_lock(_g_meta_mutex_get())
#define __ccmetaunlock pthread_mutex_unlock(_g_meta_mutex_get())
#endif

#define __ccobj(p) (ccjson_obj*)(p)

/**
 * get member index 
 */
int ccobjmindex(struct cctypemeta *meta, const char* member) {
    int idx = -1;
    if (meta->members) {
        dictEntry *entry = dictFind((dict*)meta->members, member);
        if (entry) {
            ccmembermeta *m = (ccmembermeta*)entry->v.val;
            idx = m->idx;
        }
    }
    return idx;
}

/**
 * */
struct ccmembermeta * ccobjmmetabyindex(struct cctypemeta *meta, int index) {
    int len;
    cccheckret(meta, NULL);
    len = (int)ccarraylen(meta->members);
    cccheckret(index >=0 && index<len, NULL);
    return meta->indexmembers[index];
}

// get member count of type with the meta
int ccobjmcount(struct cctypemeta *meta) {
    cccheckret(meta, 0);
    return meta->membercount;
}

/**
 * becareful the ccibool
 */
ccibool ccobjhas(void *p, int index) {
    ccjson_obj *obj = __ccobj(p);
    return (obj->__has[2*(index/8)] & (1 << (index%8))) != 0;
}

/**
 */
void ccobjset(void *p, int index) {
    ccjson_obj *obj = __ccobj(p);
    // we got value
    obj->__has[2*(index/8)] |= (1<<(index%8));
    // unset null flag
    obj->__has[1+2*(index/8)] &= ~(1<<(index%8));
}

/**
 */
void ccobjunset(void *p, int index) {
   ccjson_obj *obj = __ccobj(p);
   obj->__has[2*(index/8)] &= ~(1<<(index%8));
}

// the json object if null
ccibool ccobjisnull(void *p, int index) {
    ccjson_obj *obj = __ccobj(p);
    return (obj->__has[1+2*(index/8)] & (1 << (index%8))) != 0;
}

/**
 * */
void ccobjsetnull(void *p, int index) {
    ccjson_obj *obj = __ccobj(p);
    // set null
    obj->__has[1+2*(index/8)] |= (1<<(index%8));
    // unset has flag
    obj->__has[2*(index/8)] &= ~(1<<(index%8));
}

/**
 * */
void ccobjunsetnull(void *p, int index) {
   ccjson_obj *obj = __ccobj(p);
   obj->__has[1+2*(index/8)] &= ~(1<<(index%8));
}

// if the array have been filled the element at index
ccibool ccarrayhas(void *p, int index) {
    ccjson_obj *ip = ccarrayobj(p);
    return ccobjhas(ip, index);
}

/**
 * */
void ccarrayset(void *p, int index) {
    ccjson_obj *ip = ccarrayobj(p);
    ccobjset(ip, index);
}

/**
 * */
void ccarrayunset(void *p, int index) {
    ccjson_obj *ip = ccarrayobj(p);
    ccobjunset(ip, index);
}

// if the element at index is null
ccibool ccarrayisnull(void *p, int index) {
    ccjson_obj *ip = ccarrayobj(p);
    return ccobjisnull(ip, index);
}

/**
 * */
void ccarraysetnull(void *p, int index) {
    ccjson_obj *ip = ccarrayobj(p);
    ccobjsetnull(ip, index);
}

/**
 * */
void ccarrayunsetnull(void *p, int index) {
    ccjson_obj *ip = ccarrayobj(p);
    ccobjunsetnull(ip, index);
}


// set object is null 
void ccobjnullset(void *p, ccibool isnull) {
   ccjson_obj *obj;
   cccheck(p);
   obj = __ccobj(p);
   if (isnull) {
       obj->__flag |= enumflagccjsonobj_null;
   }else {
       obj->__flag &= ~enumflagccjsonobj_null;
   }
}


// if basic json object is null
ccibool ccobjnullis(void *p) {
   ccjson_obj *obj;
   cccheckret(p, cciyes);
   obj = __ccobj(p);
   return (obj->__flag & enumflagccjsonobj_null) != 0;
}

// init the type dict 
dict *ccgetparsedict() {
    __ccmetalock;
    if (gparses == NULL ) {
        gparses = dictCreate(&xdictTypeHeapStringCopyKey, NULL);
    }
    __ccmetaunlock;
    return gparses;
}

// add a type in dict
int ccaddtypemeta(cctypemeta *meta) {
    if (meta->index > 0) {
        return meta->index;
    }
    __ccmetalock;
    meta->index = ++gtypemetascnt;
    
    dictAdd(ccgetparsedict(), (void*)meta->type, (void*)meta);
    gtypemetas[meta->index] = meta;
    __ccmetaunlock;

    return meta->index;
}

int ccinittypemeta(cctypemeta *meta) {
    if (meta->init) {
        return meta->init(meta);
    }
    
    return ccaddtypemeta(meta);
}

// make a dict
dict *ccmakedict(cctypemeta *meta) {
   return dictCreate(&xdictTypeHeapStringCopyKey, meta);
}

/**
 * make a member meta object
 * name : member name
 * type : member type
 * offset : member offset in type
 * index : member index in type, index will be unique in one type
 * compose : if the member is array or pointer, we only support array and pointer
 * TODO: may be add support for: list, map, ...
 */
ccmembermeta *ccmakemember(const char* name, const char* type, int offset, int index, int compose) {
    return ccmakememberwithmeta(name, ccgettypemeta(type), offset, index, compose);
}

ccmembermeta *ccmakememberwithmeta(const char* name,
                           cctypemeta* type, int offset,
                           int index,
                           int compose) {
    ccmembermeta *_member = __ccmalloc(ccmembermeta);
    _member->name = name;
    _member->type = type;
    _member->offset = offset;
    _member->idx = index;
    _member->compose = compose;
    return _member;
    
}

// add a member to type(meta)
void ccaddmember(cctypemeta *meta, ccmembermeta *member) {
    unsigned long size;
    size_t len;

    if (meta->members == NULL) {
        meta->members = ccmakedict(meta);
    }
    if (meta->indexmembers == NULL) {
        if (meta->membercount <= 0) {
            meta->membercount = CCMaxMemberCount;
        }
        meta->indexmembers = (ccmembermeta**)ccarraymalloc(meta->membercount, sizeof(ccmembermeta*), 0);
    }
    // auto member index
    size = dictSize((dict*)meta->members);
    if (member->idx < (int)size) {
        member->idx = (int)size + 1;
    }
    
    // add to dict 
    dictAdd((dict*)meta->members, (void*)member->name, member);

    // find the index 
    len = ccarraylen(meta->indexmembers);
    if ((int)len > member->idx ) {
        meta->indexmembers[member->idx] = member;
    }
}

// make a type meta object
cctypemeta *ccmaketypemeta(const char* type, size_t size) {
    cctypemeta* meta = (cctypemeta*)calloc(1, sizeof(cctypemeta));
    meta->size = size;
    meta->type = type;
    meta->members = NULL;
    return meta;
}

// find type meta by type name
cctypemeta *ccgettypemeta(const char* type) {
    cctypemeta *meta = NULL;
    dictEntry *entry = NULL;

    __ccmetalock;
    entry = dictFind(ccgetparsedict(), (void*)type);
    if (entry) {
        meta = (cctypemeta*)entry->v.val;
    }
    __ccmetaunlock;

    return meta;
}

// find type meta by type index
cctypemeta *ccgettypemetaof(int index) {
    return gtypemetas[index];
}

// serial the infromation from json , will fill all the data to value
ccibool ccparse(cctypemeta *meta, void *value, cJSON *json, ccmembermeta *member) {
    // 解析
    ccibool has = ccino;
    int i;
    void **pointvalue;
    int arraysize;
    void **vv;
    void *v;
    cJSON* child;
    dictEntry *entry;
    ccmembermeta *membermeta;

    cccheckret(meta, ccino);
    cccheckret(json, ccino);
    // be sure all the meta will be init before use
    ccinittypemeta(meta);
    // array require
    if (member && member->compose == enumflagcompose_array 
            && json->type != cJSON_Array) {
        // so can not set the array be cJSON_NULL
        return has;
    }
    // point require
    if (member && member->compose == enumflagcompose_point ) {
        pointvalue = (void**)value;
        if (*pointvalue == NULL ) {
            *pointvalue = cc_alloc(meta->size);
            has = cciyes;
        }
        // deref
        value = *pointvalue;
    }

    switch(json->type) {
        case cJSON_False : {
            cccheckret(meta->type == cctypeofname(ccbool), has);
            *((ccbool*) value) = ccino;
            has = cciyes;
            break; }
        case cJSON_True : {
            cccheckret(meta->type == cctypeofname(ccbool), has);
            *((ccbool*) value) = cciyes;
            has = cciyes;
            break; }
        case cJSON_NULL : {
            // should canside the meta type
            if (meta->type == cctypeofname(ccint)) {
                *(ccint*)value = 0;
                has = cciyes;
            } else if(meta->type == cctypeofname(ccstring)) {
               // release first
               if (*(ccstring*)value) {
                   ccobjrelease(meta, value);
               }
               *(ccstring*)value = NULL;
               has = cciyes;
            } else if(meta->type == cctypeofname(ccbool)) {
                *(ccbool*) value = ccino;
                has = cciyes;
            } else if(meta->type == cctypeofname(ccnumber)) {
                *(ccnumber*)value = 0;
                has = cciyes;
            }else {
                // NB!!
                // if the array will get returned before
                // if the pointer will alloc the memory
                // any other object should be ccjson_obj
                ccobjnullset(value, cciyes);
                has = cciyes;
            }
            break; }
        case cJSON_Number : {
            cccheckret(meta->type == cctypeofname(ccint) ||
                       meta->type == cctypeofname(ccnumber) ||
                       meta->type == cctypeofname(ccint64) , has);
            if (meta->type == cctypeofname(ccint)) {
                *(ccint*)value = json->valueint;
            } else if(meta->type == cctypeofname(ccint64)) {
                *(ccint64*)value = json->valueint64;
            } else if(meta->type == cctypeofname(ccnumber)) {
                *(ccnumber*)value = json->valuedouble;
            }
            has = cciyes;
            break; }
        case cJSON_String : {
            cccheckret(meta->type == cctypeofname(ccstring), has);
            // release first
            if (*(ccstring*)value) {
                ccobjrelease(meta, value);
            }
            if (json->valuestring) {
                *(ccstring*)value = cc_dup(json->valuestring);
            } else {
                *(ccstring*)value = NULL;
            }
            has = cciyes;
            break; }
        case cJSON_Array : {
            cccheckret(member && member->compose == enumflagcompose_array, has);
            arraysize = cJSON_GetArraySize(json);
            vv = (void**)value;
            // release first
            if (*vv) {
                ccobjreleasearray(meta, value);
                ccarrayfree(*vv);
                *vv = NULL;
            }
            if (arraysize) {
                v = ccarraymalloc(arraysize, meta->size, meta->index);
                *vv = v;
                child = NULL;
                for (i=0; i<arraysize; ++i) {
                    child = cJSON_GetArrayItem(json, i);
                    // should call ccparse first
                    if (ccparse(meta, (char*)v + i * meta->size, child, NULL)) {
                        ccarrayset(v, i);
                    }
                    // set null
                    if (child->type == cJSON_NULL) {
                        ccarraysetnull(v, i);
                    }
                }
            }
            
            has = cciyes;
            break; }
        case cJSON_Object : {
            if(meta->members == NULL) {
                break;
            }
            // not null obj
            ccobjnullset(value, ccino);

            // set members
            child = json->child;
            while(child) {
                entry= dictFind((dict*)meta->members, child->string);
                if (entry) {
                    membermeta = (ccmembermeta*)entry->v.val;
                    
                    // should call ccparse first
                    if (ccparse(membermeta->type, (char*)value + membermeta->offset, child, membermeta)) {
                        ccobjset(value, membermeta->idx);
                    }
                    // set null
                    if (child->type == cJSON_NULL) {
                        ccobjsetnull(value, membermeta->idx);
                    } 
                }
                child = child->next;
            }
            has = cciyes;
            break; }
    }
    
    return has;
}

// forward declare
ccibool ccunparsemember(ccmembermeta *mmeta, void *value, struct cJSON *json);

// unserial the json object to a json string, returned string neededcall cc_free to free the memory
cJSON *ccunparse(cctypemeta *meta, void *value) {
    cJSON *obj = NULL;
    dictIterator *ite;
    dictEntry *entry;
    ccmembermeta* member;
    ccbool *b;
    ccint *i;
    ccint64 *i64;
    ccnumber *n;
    ccstring *s;

    // may be null
    cccheckret(value, NULL);
    // be sure all the meta will be init before use
    ccinittypemeta(meta);
    // unserial
    if (meta->members) {
        if (ccobjnullis(value) ) {
            obj = cJSON_CreateNull();
        } else {
            obj = cJSON_CreateObject();
            ite = dictGetIterator((dict*)meta->members);
            entry = dictNext(ite);
            while (entry) {
                member = (ccmembermeta*)entry->v.val;
                if (ccobjhas(value, member->idx)) {
                    ccunparsemember(member, (char*)value + member->offset, obj);
                }
                entry = dictNext(ite);
            }
            dictReleaseIterator(ite);
        }
    } else if (meta->type == cctypeofname(ccbool)) {
        b = (ccbool*)value;
        obj = cJSON_CreateBool(*b);
    } else if(meta->type == cctypeofname(ccint)) {
        i = (ccint*)value;
        obj = cJSON_CreateNumber(*i);
    } else if (meta->type == cctypeofname(ccint64)) {
        i64 = (ccint64*)value;
        obj = cJSON_CreateNumber64(*i64);
    } else if(meta->type == cctypeofname(ccnumber)) {
        n = (ccnumber*)value;
        obj = cJSON_CreateNumber(*n);
    } else if(meta->type == cctypeofname(ccstring)) {
        s = (ccstring *)value;
        if(*s) {
            obj = cJSON_CreateString(*s);
        }else {
            obj = cJSON_CreateNull();
        }
    } else {
      // todo: @@
    }
    return obj;
}

// unserial a member
ccibool ccunparsemember(ccmembermeta *mmeta, void *value, struct cJSON *json) {
    cctypemeta *meta;

    void **arrayvalue;
    void *varray;
    char *v;

    int len;
    int i;
    cJSON *array;
    cJSON * obj;

    void** pointvalue;

    cccheckret(mmeta, ccino);
    cccheckret(json, ccino);
    meta = mmeta->type;
    cccheckret(meta, ccino);
    // unparse
    if (mmeta->compose == enumflagcompose_array) {
        arrayvalue = (void**)value;
        varray = *arrayvalue;
        len = (int)ccarraylen(varray);
        array = cJSON_CreateArray();
        if (len) {
            v = (char*)(varray);
            for (i=0; i < len; ++i) {
                obj = NULL;
                // null
                if (ccarrayisnull(varray, i)) {
                    obj = cJSON_CreateNull();
                }else if (ccarrayhas(varray, i) ) {
                    obj = ccunparse(meta, v + i * meta->size);
                }
                if (obj) {
                    cJSON_AddItemToArray(array, obj);
                }
            }
        }
        
        cJSON_AddItemToObject(json, mmeta->name, array);
    }else {
        // if compose point need deref
        if (mmeta->compose == enumflagcompose_point) {
            pointvalue = (void**)value;
            value = *pointvalue;
        }
        obj = ccunparse(meta, value);
        if (obj) {
            cJSON_AddItemToObject(json, mmeta->name, obj);
        }
    }
    return cciyes;
}

// forward declare
ccibool ccobjreleasemember(ccmembermeta *mmeta, void *value);

// release the memory hold by p with type meta
// the json object that have been called from ccparsefrom need call this to free memory 
void ccobjrelease(cctypemeta *meta, void *value) {
    dictIterator *ite;
    dictEntry *entry;
    ccmembermeta* member;
    ccstring * s;

    // release
    if (meta->members) {
        ite = dictGetIterator((dict*)meta->members);
        entry = dictNext(ite);
        while (entry) {
            member = (ccmembermeta*)entry->v.val;
            if (ccobjhas(value, member->idx)) {
                ccobjreleasemember(member, (char*)value + member->offset);
                ccobjunset(value, member->idx);
            }
            entry = dictNext(ite);
        }
        dictReleaseIterator(ite);
    } else if (meta->type == cctypeofname(ccbool)) {
        //bool *b = (bool*)value;
    } else if(meta->type == cctypeofname(ccint)) {
        //ccint *i = (ccint*)value;
    } else if(meta->type == cctypeofname(ccnumber)) {
        //ccnumber *n = (ccnumber*)value;
    } else if(meta->type == cctypeofname(ccstring)) {
        s = (ccstring *)value;
        if (*s) {
            cc_free(*s);
            *s = NULL;
        }
    } else {
        // todo: @@
    }
}

// release the memory hold by value with array type meta 
void ccobjreleasearray(cctypemeta* meta, void *value) {
    int i, len;
    char *v;
    void **arrayvalue = (void**)value;
    if (*arrayvalue) {
        len = (int)ccarraylen(*arrayvalue);
        if (len) {
            v = (char*)(*arrayvalue);
            for (i=0; i < len; ++i) {
                ccobjrelease(meta, v + i * meta->size);
            }
        }
    }
}

// release the member
ccibool ccobjreleasemember(ccmembermeta *mmeta, void *value) {
    void **pointvalue;
    void **arrayvalue;
    cctypemeta *meta;

    cccheckret(mmeta, ccino);
    meta = mmeta->type;
    cccheckret(meta, ccino);
    // release
    if (mmeta->compose == enumflagcompose_array) {
        ccobjreleasearray(meta, value);

        // free array
        arrayvalue = (void**)value;
        ccarrayfree(*arrayvalue);
        *arrayvalue = NULL;
    }else {
        pointvalue = NULL;
        // deref
        if (mmeta->compose == enumflagcompose_point) {
            pointvalue = (void**)value;
            value = *pointvalue;
        }
        
        ccobjrelease(meta, value);
        
        // free obj
        if (pointvalue) {
            cc_free((char*)(*pointvalue));
            *pointvalue = NULL;
        }
    }
    return cciyes;
}

// serial the infromation from json , will fill all the data to value
ccibool ccparsefrom(cctypemeta *meta, void *value, const char *json) {
    ccibool ok = ccino;
    cJSON* cjson = cJSON_Parse(json);
    ok = ccparse(meta, value, cjson, NULL);
    cJSON_Delete(cjson);
    return ok;
}

// unserial the json object to a json string, returned string neededcall cc_free to free the memory
char *ccunparseto(cctypemeta *meta, void *value) {
    char *str = NULL;
    char *content = NULL;
    cJSON *json = ccunparse(meta, value);
    str = cJSON_Print(json);
    cJSON_Delete(json);
    content = cc_dup(str);
    free(str);
    return content;
}

// set the meta index in basic json object
#define __cc_setmetaindex(p, index) do { \
    ccjson_obj* obj = (ccjson_obj*)p; \
    if(obj && obj->__index == index) { \
        obj->__index = index; \
    }} while(0)  

// ******************************************************************************
// helper: malloc a basic json object with type meta, we can call the ccjsonobjfree to free memories
void *ccjsonobjalloc(cctypemeta* meta) {
    ccjson_obj *obj;
    ccinittypemeta(meta);

    obj = (ccjson_obj*)cc_alloc(meta->size);
    obj->__index = meta->index;
    __cc_setflag(obj, __CC_JSON_OBJ);
    return obj;
}

// helper: free the memories holded by p, but not free p self
void ccjsonobjrelease(void *p) {
    ccjson_obj *obj;
    cctypemeta *meta;

    cccheck(p);
    obj = (ccjson_obj*)p;
    meta = ccgettypemetaof(obj->__index);
    ccobjrelease(meta, obj);
}

// helper: free the memories holded by p, and then free p self
void ccjsonobjfree(void *p) {
    if (__cc_hasflag(p, __CC_JSON_OBJ) ) {
        ccjsonobjrelease(p);
        cc_free((char*)p);
    }else if (__cc_hasflag(p, __CC_JSON_ARRAY) ) {
        ccarrayfree(p);
    }else {
        cc_free((char*)p);
    }
}

// helper: serial from json 
ccibool ccjsonobjparsefrom(void *p, const char* json) {
    ccjson_obj *obj = (ccjson_obj*)p;
    cctypemeta *meta = ccgettypemetaof(obj->__index);
    return ccparsefrom(meta, p, json);
}

// helper: unserial to json
char* ccjsonobjunparseto(void *p) {
    ccjson_obj *obj = (ccjson_obj*)p;
    cctypemeta *meta = ccgettypemetaof(obj->__index);
    return ccunparseto(meta, p);
}

// implement the basic json type
// ******************************************************************************

// basci types
__ccimplementtype(ccint)
__ccimplementtype(ccint64)
__ccimplementtype(ccnumber)
__ccimplementtype(ccstring)
__ccimplementtype(ccbool)


// implement the example the complex type
__ccimplementtypebegin(ccconfig)
__ccimplementmember(ccconfig, ccint, ver)
__ccimplementmember(ccconfig, ccint64, ver64)
__ccimplementmember(ccconfig, ccbool, has)
__ccimplementmember(ccconfig, ccstring, detail)
__ccimplementmember_array(ccconfig, ccint, skips)
__ccimplementtypeend(ccconfig)

