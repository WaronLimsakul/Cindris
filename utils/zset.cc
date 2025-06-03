#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "buffer.h"
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

// pair with znode_new (malloc-free pair)
static void znode_destroy(ZNode *node) {
    free(node);
}


// compare HNode from ZNode with HNode from HKey
static bool hnodecmp(HNode *target, HNode *ref) {
    ZNode *znode = container_of(target, ZNode, hnode);
    HKey *hkey = container_of(ref, HKey, hnode);
    if (hkey->len != znode->len) return false;
    return 0 == memcmp(znode->name, hkey->name, hkey->len);
}

// just hashmap look up using tuple comparison
// 1. hmap_lookup need a compare function, write one 
// 2. get the hnode
// 3. return container pointer
ZNode *zset_lookup(ZSet *zset, const char *name, size_t len) {
    // if zset or root dne, don't bother
    if (!zset || !zset->root) {
        return NULL;
    }

    // create a reference for comparing
    HKey ref = {.hnode = HNode{}, .len = len, .name = name};
    ref.hnode.hashval = str_hash((uint8_t *)name, len);

    HNode *hnode_res = hm_lookup(&zset->map, &ref.hnode, &hnodecmp);
    return hnode_res ? container_of(hnode_res, ZNode, hnode) : NULL;
}

// tuple comparison:
// 1. compare score first
// 2. then compare string name
// 3. if still equal, check the len
static bool zless(AVLNode *node1, AVLNode *node2) {
    assert(node1 && node2);
    ZNode *z1 = container_of(node1, ZNode, avlnode);
    ZNode *z2 = container_of(node2, ZNode, avlnode);
    
    if (z1->score < z2->score) return true;
    if (z1->score > z2->score) return false;

    int rv = memcmp(z1->name, z2->name, min(z1->len, z2->len));
    if (rv != 0) {
        return rv < 0;
    }

    return z1->len < z2->len;
}

// insert the avlnode into the tree in zset and update
// 1. get the avl node, assert exist
// 2. get the tree root
// 3. traverse to get the position we want (AVLNode **)
//  - we can't just compare anything, we need tuple comparison
// 4. set target node and its parent
// 5. update the root by avl_fix(inserted_node);
static void zset_tree_insert(ZSet *zset, ZNode *node) { 
    assert(node && &node->avlnode); 
    AVLNode *new_node = &node->avlnode;

    AVLNode **cur = &zset->root;
    // if no root, we assign this as new node's parent
    AVLNode *parent = NULL; 
    while (*cur) {
        parent = *cur;
        cur = zless(new_node, *cur) ? &parent->left : &parent->right;
    }

    *cur = new_node;
    new_node->parent = parent; // parent = the node before cur
    zset->root = avl_fix(new_node);
}

// take a szet, original znode and new score
// 1. delete the avlnode of znode from zset
// 2. reset original avlnode using avl_init()
// 3. update new znode to have new score
// 4. insert this node into szet's tree
// note: we don't deal with hashmap because the hnode is still the same
// = pointer next + hashval (hash of name, which doesn't change)
static void zset_update(ZSet *zset, ZNode *znode, double new_score) {
    zset->root = avl_del(&znode->avlnode);     
    avl_init(&znode->avlnode);
    znode->score = new_score;
    zset_tree_insert(zset, znode);
}

// insert new pair /update score in sorted set
// 0. check first if name already exists, if yes: update
// 1. create a new znode
// 2. insert new znode into zset's tree
// 3. insert new znode into zset's hashmap
bool zset_insert(ZSet *zset, const char *name, size_t len, double score) {
    assert(zset);
    ZNode *target;
    if ((target = zset_lookup(zset, name, len)) != NULL) {
        zset_update(zset, target, score);
        return false; // insert fail, but still update
    }
    ZNode *new_node = znode_new(name, len, score);
    zset_tree_insert(zset, new_node);
    hm_insert(&zset->map, &new_node->hnode);
    return true;
}

// receive a zset and (can be dummy) znode => delete it from zset + free memory
// 1. delete znode's avlnode from zset tree
// 2. delete znode's hnode from zset map
//  - put HKey dummy in hm_delete() so we can just use hnodecmp()
// 3. free the memory for detached node
void zset_delete(ZSet *zset, ZNode *node) {
    assert(zset && node);
    zset->root = avl_del(&node->avlnode);

    HKey dummy = HKey {
        .hnode = node->hnode, 
        .len = node->len,
        .name = node->name, 
    };
    HNode *detached_hnode = hm_delete(&zset->map, &dummy.hnode, &hnodecmp); 
    assert(detached_hnode);
    znode_destroy(container_of(detached_hnode, ZNode, hnode));
}

// The range query command: ZQUERY key score name offset limit.
//  Seek to the first pair where pair >= (score, name).
//  Walk to the n-th successor/predecessor (offset).
//  Iterate and output.

// tree search from zset using (score, name) to find first pair where >= (score, name)
// note: we use normal bst AND found = cur; to acheive
// effectively samething as search range BST (the mid = l + (r - l) / 2)
ZNode *zset_seekge(ZSet *zset, double score, const char *name, size_t len) {
    assert(zset);
    if (!zset->root) return NULL;

    ZNode *new_znode = znode_new(name, len, score);
    ZNode *found = NULL;
    AVLNode *cur_avl = zset->root;
    while (cur_avl) {
        ZNode *cur_z = container_of(cur_avl, ZNode, avlnode);
        if (zless(cur_avl, &new_znode->avlnode)) {
            cur_avl = cur_avl->left;
        } else {
            found = cur_z; // this pattern is crazy!!
            cur_avl = cur_avl->right;
        }
    }
    znode_destroy(new_znode);
    return found;
}

// receive a znode and offset, return the forward or 
// backward offset znode* depends on offset.
ZNode *znode_offset(ZNode *znode, size_t offset) {
    AVLNode *target_avl = avl_offset(&znode->avlnode, offset);
    return target_avl ? container_of(target_avl, ZNode, avlnode) : NULL;
}

