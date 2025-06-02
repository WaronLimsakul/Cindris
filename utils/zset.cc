#include <stdlib.h>
#include <string.h>

#include "zset.h"

// initialize new node with correct value
// 1. malloc new node
// 2. init the tree node
// 3. hash the name and put it in map node
// 4. assign len and score
// 5. assign name at the end (copy) 
static ZNode *znode_new(const char *name, size_t len, double score) {
    // we use flexible array, which C++'s `new` doesn't know
    // so we have to use malloc.
    ZNode *znode = (ZNode *)malloc(sizeof(ZNode) + len);

    // node part
    avl_init(&znode->avlnode);
    znode->hnode.hashval = str_hash((uint8_t *)name, len);

    // wrapping data part
    znode->len = len;
    znode->score = score;
    memcpy(&znode->name, name, len);
    return znode;
}

// pairt with znode_new (malloc-free pair)
static void znode_destory(ZNode *node) {
    free(node);
}


// compare HNode from ZNode with HNode from HKey
static bool hnodecmp(HNode *target, HNode *ref) {
    ZNode *znode = container_of(target, ZNode, hnode);
    HKey *hkey = container_of(ref, HKey, hnode);
    if (hkey->len != znode->len) return false;
    return 0 == memcmp(znode->name, hkey->name, hkey->len);
}

// just hashmap look up
// plan
// 1. hmap_lookup need a compare function, write one 
// 2. get the hnode
// 3. return container pointer
ZNode *zset_lookup(ZSet *zset, const char *name, size_t len) {
    // if root dne, don't bother
    if (!zset->root) {
        return NULL;
    }

    // create a reference for comparing
    HKey ref = {.hnode = HNode{}, .name = name, .len = len};
    ref.hnode.hashval = str_hash((uint8_t *)name, len);

    HNode *hnode_res = hm_lookup(&zset->map, &ref.hnode, &hnodecmp);
    return hnode_res ? container_of(hnode_res, ZNode, hnode) : NULL;
}

