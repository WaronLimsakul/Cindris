#pragma once

#include "hashtable.h"
#include "avltree.h"

struct ZSet {
    AVLNode *root = NULL; // tree: score -> name
    HMap map; // hashmap: name -> score
};

struct ZNode {
    AVLNode avlnode;
    HNode hnode;

    double score;
    size_t len; // name len
    char name[0]; // flexible array
};

// helper struct for comparing HNode
static struct HKey {
    HNode hnode;
    size_t len;
    char *name;
}

// just insert a new data into our set. return false if data already exist
bool zset_insert(ZSet *zset, const char *name, size_t len, double score);
ZNode *zset_lookup(ZSet *zset, const char *name, size_t len); // look up by name
void zset_delete(ZSet *zset, ZNode *node); // delete from complete node
