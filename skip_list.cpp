#include <stdlib.h>
#include <time.h>
#include "skip_list.hpp"

static const size_t MAX_LEVEL = 32;        

void sl_init(SList *list, bool (*less_cmp)(SLNode *a, SLNode *b)) {
    static bool seeded = false;
    if(!seeded) {
        srand((unsigned)time(NULL));
        seeded = true;
    }

    list->less_cmp = less_cmp;
    list->head = new SLNode();
    list->level = 0;
    list->size = 0;

    list->head->forward = new SLNode*[MAX_LEVEL];           //Mby malloc
    for(int i = 0; i < MAX_LEVEL; i++) {
        list->head->forward[i] = NULL;
    }
}

static size_t sl_get_random_level() {
    size_t level = 1;
    while(rand() > RAND_MAX / 2 && level < MAX_LEVEL) {         //Fixed max levels
        level++;
    }

    return level;
}

SLNode *sl_insert(SList *list, SLNode *node) {
    SLNode *update[MAX_LEVEL];
    SLNode *existing = sl_find_path(list, node, update);

    //Replace existing node behaviour
    SLNode *replaced_node = NULL;
    if(existing != NULL) {
        replaced_node = sl_detach(list, existing, update);
    }

    size_t new_level_height = sl_get_random_level();
    size_t new_level_indx = new_level_height - 1;

    node->level = new_level_indx;
    node->forward = new SLNode*[new_level_height];      //Mby malloc
    
    if(new_level_indx > list->level) {
        for(int i = list->level + 1; i <= new_level_indx; i++) {
            update[i] = list->head;
        }

        list->level = new_level_indx;
    }

    for(int i = 0; i <= new_level_indx; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
    }

    list->size++;
    return replaced_node;
}

static SLNode *sl_find_path(SList *list, SLNode *key, SLNode **update) {
    SLNode *current = list->head;
    
    for(int i = list->level; i >= 0; i--) {
        while(current->forward[i] != NULL && list->less_cmp(current->forward[i], key)) {
            current = current->forward[i];
        }

        if(update) update[i] = current;
    }

    SLNode *target = current->forward[0];
    if(target != NULL && !list->less_cmp(target, key) && !list->less_cmp(key, target)) {
        return target; 
    }

    return NULL;
}

SLNode *sl_find(SList *list, SLNode *key) {
    return sl_find_path(list, key, NULL);
}

static SLNode *sl_detach(SList *list, SLNode *target, SLNode **update) {
    for(int i = target->level; i >= 0; i--) {
        if(update[i]->forward[i] == target) update[i]->forward[i] = target->forward[i];
    }

    delete[] target->forward;
    target->forward = NULL;

    while(list->level > 0 && list->head->forward[list->level] == NULL) {
        list->level--;
    }

    list->size--;
    return target;
}

SLNode *sl_delete(SList *list, SLNode *key) {
    SLNode *update[MAX_LEVEL];      //Could also use "list->level + 1" as array size; Used MAX_LEVEL for consistency
    SLNode *target = sl_find_path(list, key, update);

    if(target == NULL) return NULL;

    return sl_detach(list, target, update);
}

void sl_clear(SList *list) {
    SLNode *current = list->head->forward[0];

    while(current != NULL) {
        SLNode *next = current->forward[0];

        delete[] current->forward;
        current->forward = NULL;

        current = next;
    }

    for(int i = 0; i <= list->level; i++) {
        list->head->forward[i] = NULL;
    }

    list->level = 0;
    list->size = 0;
}

size_t sl_size(SList *list) {
    return list->size;
}
