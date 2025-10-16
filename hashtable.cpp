#include <stdlib.h>
#include <assert.h>
#include "hashtable.hpp"

static void h_init(HTab *htab, size_t n) {
    assert(n > 0 && ((n - 1) & n) == 0);            //n must be a power of 2 (To avoid modulo for slot)
    htab->tab = (HNode **)calloc(n, sizeof(HNode *));
    htab->mask = n - 1;
    htab->size = 0;
}

//Hashtable/Linked list insertion
static void h_insert(HTab *htab, HNode *node) {
    size_t pos = htab->mask & node->hcode;          //Same as: node->hcode & (n - 1) OR hash(key) % N OR hash(key) & (N - 1)
    HNode *next = htab->tab[pos];            //'pos' is the slot number; 'tab' is a 2d array (double pointer); Each slot can further have chaining thus 2d array
    node->next = next;
    htab->tab[pos] = node;
    htab->size++;
}

//Hashtable lookup subroutine
//This function returns the address of the parent pointer that owns the target node,
//which can be used to delete the target node
static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)) {
    if(!htab->tab) {
        return NULL;
    }

    size_t pos = htab->mask & key->hcode;
    HNode **from = &htab->tab[pos];                 //Incoming pointer to the target
    for(HNode *cur; (cur = *from) != NULL; from = &cur->next) {
        if(cur->hcode == key->hcode && eq(cur, key)) {
            return from;                    //We return 'from' instead of 'cur', cuz deletion also required
        }                               //Here 'cur' contains the address of the node found and 'from' -
    }                                   //-contains the address of the parent pointer of the node found
                                        //'from' may be a node, may be a slot (slot in case the node to be removed is the first node)
    return NULL;
}

//To remove a node from the chain
static HNode *h_detach(HTab *htab, HNode **from) {
    HNode *node = *from;                //The target node
    *from = node->next;                 //Update incoming pointer to the target; i.e. Making *from point to the next in line
    htab->size--;
    return node;
}

static void hm_trigger_rehashing(HMap *hmap) {
    assert(hmap->older.tab == NULL);
    hmap->older = hmap->newer;          //(newer, older) <- (new_table, newer)
    h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
    hmap->migrate_pos = 0;              //Set migrate_pos to 0 to reset before every rehashing
}

const size_t k_rehashing_work = 128;        //Max amount of rehashing work to be done per 'get', 'set', 'del' commands

static void hm_help_rehashing(HMap *hmap) {
    size_t work_done = 0;
    while(work_done < k_rehashing_work && hmap->older.size > 0) {
        //Find a non empty slot to start transferring nodes
        HNode **from = &hmap->older.tab[hmap->migrate_pos];
        if(!*from) {
            hmap->migrate_pos++;            //We change the slot only when all the nodes in it have been transferred
            continue;                   //Slot empty, so go to next slot
        }

        //Detach the node/item from older table and then insert it into the newer table
        h_insert(&hmap->newer, h_detach(&hmap->older, from));
        work_done++;
    }

    //Once older hashtable is empty, discard/free it
    if(hmap->older.size == 0 && hmap->older.tab) {
        free(hmap->older.tab);
        hmap->older = HTab {};              //Allocate memory to older hashtable but keep it empty
    }
}

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_rehashing(hmap);
    HNode **from = h_lookup(&hmap->newer, key, eq);
    if(!from) {
        from = h_lookup(&hmap->older, key, eq);
    }

    return from ? *from : NULL;
}

HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_rehashing(hmap);
    if(HNode **from = h_lookup(&hmap->newer, key, eq)) {
        return h_detach(&hmap->newer, from);
    }
    if(HNode **from = h_lookup(&hmap->older, key, eq)) {
        return h_detach(&hmap->older, from);
    }

    return NULL;
}

const size_t k_max_load_factor = 8;         //Load Factor = (Number of elements or values) / (Number of slots)

void hm_insert(HMap *hmap, HNode *node) {
    if(!hmap->newer.tab) {
        h_init(&hmap->newer, 4);            //Initialise the hashtable if it is empty
    }
    h_insert(&hmap->newer, node);           //Always insert values into the newer table (Larger one)

    //Checking if we need to rehash; if hmap->older.tab is not NULL, that means rehashing is going on; 
    //If it is NULL, that means rehashing is complete and we can rehash again if needed
    if(!hmap->older.tab) {                  
        size_t threshold = (hmap->newer.mask + 1) * k_max_load_factor;
        if(hmap->newer.size >= threshold) {
            hm_trigger_rehashing(hmap);
        }
    }

    hm_help_rehashing(hmap);                //Migrate/transfer some keys over each time (from older to newer)
}

void hm_clear(HMap *hmap) {
    free(hmap->newer.tab);
    free(hmap->older.tab);
    *hmap = HMap {};
}

size_t hm_size(HMap *hmap) {
    return hmap->newer.size + hmap->older.size;
}

static bool h_foreach(HTab *htab, bool (*cb)(HNode *, void *), void *arg) {
    for(size_t i = 0; htab->mask != 0 && i <= htab->mask; i++) {
        for(HNode *node = htab->tab[i]; node != NULL; node = node->next) {
            if(!cb(node, arg)) {
                return false;
            }
        }
    }

    return true;
}

void hm_foreach(HMap *hmap, bool (*cb)(HNode *, void *), void *arg) {
    h_foreach(&hmap->newer, cb, arg) && h_foreach(&hmap->older, cb, arg);
}
