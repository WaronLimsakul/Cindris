#include <stddef.h>
#include "avltree.h"

// NULl-safe
static uint32_t avl_height(AVLNode *node) {
    return node ? node->height : 0; 
}

static uint32_t max(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

// update the height
static void avl_update(AVLNode *node) {
    if (!node) return;
    node->height = max(avl_height(node->left), avl_height(node->right)) + 1;
}

static AVLNode *rotate_left(AVLNode *node) {
    assert(node);
    AVLNode *new_node = node->right;
    assert(new_node);
    AVLNode *parent = node->parent;
    AVLNode *internal_node = new_node->left;
    // internal <-> node
    node->right = internal_node;
    if (internal_node) {
        internal_node->parent = node;
    }

    // new_node goes up, node goes down
    new_node->parent = parent;
    new_node->left = node;
    node->parent = new_node;
    return new_node; // so we can update parent's child
}

static AVLNode *rotate_right(AVLNode *node) {
    assert(node);
    AVLNode *new_node = node->left;
    assert(new_node);
    AVLNode *parent = node->parent;
    AVLNode *internal_node = new_node->right;
    // internal <-> node
    node->left = internal_node;
    if (internal_node) {
        internal_node->parent = node;
    }

    // new_node goes up, node goes down
    new_node->parent = parent;
    new_node->right = node;
    node->parent = new_node;

    // update height
    avl_update(new_node);
    avl_update(node);
    return new_node; // so we can update parent's child
}

// for a node, if the h_left - h_right = 2, we call this function 
static AVLNode *avl_fix_left(AVLNode *node) {
    if (avl_height(node->left->right) > avl_height(node->left->left)) {
        node->left = rotate_left(node->left);
    } 
    return rotate_right(node);
}

// for a node, if the h_right - h_left = 2, we call this function 
static AVLNode *avl_fix_right(AVLNode *node) {
    if (avl_height(node->left->left) > avl_height(node->left->right)) {
        node->right = rotate_right(node->right);
    } 
    return rotate_left(node);
}

// Called on an updated node:
// - Propagate auxiliary data.
// - Fix imbalances.
// - Return the new root node.
AVLNode *avl_fix(AVLNode *node) {
    AVLNode *prev = NULL;
    for (AVLNode *cur = node; cur != NULL; cur = cur->parent) {
        AVLNode *parent = cur->parent;
        // want this to be a place that parent store pointer to new child
        AVLNode **to_update = &cur; // safe init
        if (parent) {
            to_update = parent->left == cur ? &parent->left : &parent->right;
        }

        // in prev iteration, we just fix child height 
        // so we have to update ourselves
        avl_update(cur);

        uint32_t hright = avl_height(cur->right);
        uint32_t hleft = avl_height(cur->left);

        // update new child for parent
        if (hright - hleft == 2) {
            *to_update = avl_fix_right(cur);
        } else if (hleft - hright == 2) {
            *to_update = avl_fix_left(cur);
        }
        prev = *to_update; // we want return new root node
    }
    return prev;
}

static AVLNode *avl_del_easy(AVLNode *node) {
    AVLNode *new_node = node->left != NULL ? node->left : node->right;
    AVLNode *parent = node->parent;
    if (new_node) {
        new_node->parent = parent;
    }

    // if root, return new root
    if (!parent) {
        return new_node; 
    }

    AVLNode **from = parent->left == node ? &parent->left : &parent->right;
    *from = new_node;
    return avl_fix(parent);
}

AVLNode *avl_del(AVLNode *node) {
    if (!node->left || !node->right) {
        return avl_del_easy(node);
    }

    AVLNode *right_node = node->right;
    AVLNode *successor = right_node;
    while (successor->left) {
        successor = successor->left;
    }

    AVLNode *new_root = avl_del_easy(successor);
    *successor = *node;    
    if (successor->left) {
        successor->left->parent = successor;
    }
    if (successor->right) {
        successor->right->parent = successor;
    }
    AVLNode *parent = node->parent;
    if (parent) {
        AVLNode **from = parent->left == node ? &parent->left : &parent->right;
        *from = successor;
    }

    return new_root;
}
