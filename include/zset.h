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
struct HKey {
    HNode hnode;
    size_t len;
    const char *name;
};

// just insert a new data into our set. return false if data already exist
bool zset_insert(ZSet *zset, const char *name, size_t len, double score);

// just hashmap look up using tuple comparison
ZNode *zset_lookup(ZSet *zset, const char *name, size_t len); // look up by name

// receive a zset and (can be dummy) znode => delete it from zset + free memory
void zset_delete(ZSet *zset, ZNode *node); // delete from complete node

// tree search from zset using (score, name) to find first pair where >= (score, name)
// (ge stands for greater than or equal btw)
ZNode *zset_seekge(ZSet *zset, double score, const char *name, size_t len);

// receive a znode and offset, return the forward or 
// backward offset znode* depends on offset.
ZNode *znode_offset(ZNode *znode, size_t offset);

