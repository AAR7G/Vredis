#pragma once

#include <stdint.h>
#include <stddef.h>

//Hashtable node; Is embedded into the payload
struct HNode {                      //Intrusive linked list node
    struct HNode *next = NULL;
    uint64_t hcode = 0;             //Hash value of the key
};

//Fixed size hashtable
struct HTab {
    struct HNode **tab = NULL;      //Array of slots
    size_t mask = 0;                //Power of 2 array size (2^n - 1)
    size_t size = 0;                //Number of keys
};

//The actual hashtable interface
//It uses 2 fixed size hashtables for progressive rehashing
struct HMap {
    struct HTab older;
    struct HTab newer;
    size_t migrate_pos = 0;
};

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_insert(HMap *hmap, HNode *node);
HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_clear(HMap *hmap);
size_t hm_size(HMap *hmap);
void hm_foreach(HMap *hmap, bool (*cb)(HNode *, void *), void *arg);    //Invoke callback on each node, until false
