#pragma once

#include <stdint.h>
#include <stddef.h>

//Intrusive skip list node
struct SLNode {
    struct SLNode **forward;    //Array of pointers; forward[i] refers to next node in ith layer/level
    int level;                  //Number of levels the node is a part of, to know size of forward array
};

//Actual skip list data structure
struct SList {
    struct SLNode *head;    //Sentinel head node pointer. Start line/fixed entry point of entire skip list
    size_t size;            //Number of elements
    int level;              //The max level of any node in list
    bool (*less_cmp)(SLNode *a, SLNode *b);     //A generic "less than" comparision function to traverse the list
};

void sl_init(SList *list, bool (*less_cmp)(SLNode *a, SLNode *b));
SLNode *sl_find(SList *list, SLNode *key);
SLNode *sl_insert(SList *list, SLNode *node);
SLNode *sl_delete(SList *list, SLNode *key);
void sl_clear(SList *list);
size_t sl_size(SList *list);
