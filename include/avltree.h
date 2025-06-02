#pragma once
#include <stdint.h>
#include <assert.h>

struct AVLNode {
    AVLNode *parent = NULL;
    AVLNode *left = NULL;
    AVLNode *right = NULL;
    uint32_t height = 0; // len of longest path from leaf to that node
    uint32_t count = 0; // # nodes in subtree
};

// make the node exist
inline void avl_init(AVLNode *root) {
    assert(root);
    root->parent = root->left = root->right = NULL;
    root->height = 1;
    root->count = 1;
};

// NULl-safe
inline uint32_t avl_height(AVLNode *node) {
    return node ? node->height : 0; 
}

inline uint32_t avl_count(AVLNode *node) {
    return node ? node->count : 0; 
}

inline uint32_t max(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

// called on the parent node after an insert to restore balance
AVLNode *avl_fix(AVLNode *node);

// detach a node, this already use avl_fix
AVLNode *avl_del(AVLNode *node); 

