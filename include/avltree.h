#pragma once
#include <stdint.h>
#include <assert.h>

struct AVLNode {
    AVLNode *parent = NULL;
    AVLNode *left = NULL;
    AVLNode *right = NULL;
    uint32_t height = 0;
};

// make the node exist
inline void avl_init(AVLNode *root) {
    assert(root);
    root->parent = root->left = root->right = NULL;
    root->height = 1;
};

// called on the parent node after an insert to restore balance
AVLNode *avl_fix(AVLNode *node);

// detach a node, this already use avl_fix
AVLNode *avl_del(AVLNode *node); 

