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

    list->head->forward = new SLNode*[MAX_LEVEL];
    for(int i = 0; i < MAX_LEVEL; i++) {
        list->head->forward[i] = NULL;
    }
}

static size_t sl_get_random_level() {
    size_t level = 0;
    while(rand() > RAND_MAX / 2 && level < MAX_LEVEL) {         //Fixed max levels
        level++;
    }

    return level;
}

void sl_insert(SList *list, SLNode *node) {
    SLNode *current = list->head;
    SLNode *update[MAX_LEVEL];

    for(int i = list->level; i >= 0; i--) {
        while(current->forward[i] != NULL && list->less_cmp(current->forward[i], node)) {
            current = current->forward[i];
        }

        update[i] = current;
    }

    //replace behaviour

    size_t new_level_height = sl_get_random_level();
    size_t new_level_indx = new_level_height - 1;

    node->level = new_level_indx;
    node->forward = new SLNode*[new_level_height];      //
    
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
}

