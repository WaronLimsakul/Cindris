#include <stddef.h>

#include "common.h"
#include "avltree.h"


// update the height and count
static void avl_update(AVLNode *node) {
    if (!node) return;
    node->height = max(avl_height(node->left), avl_height(node->right)) + 1;
    node->count = avl_count(node->left) + avl_count(node->right) + 1;
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

    // update at the end (node first because it's below new_node)
    avl_update(node);
    avl_update(new_node);
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
    avl_update(node);
    avl_update(new_node);
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
    if (avl_height(node->right->left) > avl_height(node->right->right)) {
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
        prev = *to_update; // we want to return new root node
    }
    return prev;
}

// delete target node and return a new root
static AVLNode *avl_del_easy(AVLNode *node) {
    assert(!node->left || !node->right);

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
    // easy case
    if (!node->left || !node->right) {
        return avl_del_easy(node);
    }

    // find sucessor
    AVLNode *successor = node->right;
    while (successor->left) {
        successor = successor->left;
    }

    // detach successor
    AVLNode *root = avl_del_easy(successor);

    // update successor's fields 
    *successor = *node;    

    // update its relative
    if (successor->left) {
        successor->left->parent = successor;
    }
    if (successor->right) {
        successor->right->parent = successor;
    }

    // update new_node's parent. If node is root, then we update the root
    AVLNode *parent = node->parent;
    AVLNode **from = &root;
    if (parent) {
        from = parent->left == node ? &parent->left : &parent->right;
    }
    *from = successor;
    
    return root;
}

// return the next node in order
static AVLNode *successor(AVLNode *node) {
    assert(node);
    // if has right, return left most in the right sub-tree
    if (node->right) {
        AVLNode *cur = node->right;           
        while (cur->left != NULL) {
            cur = cur->left;
        }
        return cur;
    } 
    // if has no right, it might be in left sub-tree, traverse to get the parent 
    for (AVLNode *cur = node; cur != NULL; cur = cur->parent) {
        AVLNode *parent = cur->parent;
        if (parent->left == cur) {
            return parent;
        }
    }
    // it is at the most value node
    return NULL;
}

// return thet node before in order
static AVLNode *predecessor(AVLNode *node) {
    assert(node); 
    // if has left, return right most in the left sub-tre
    if (node->left) {
        AVLNode *cur = node->left;
        while (cur->right != NULL) {
            cur = cur->left;
        }
        return cur;
    }
    // if has no left, it still might be in right sub-tree
    for (AVLNode *cur = node; cur != NULL; cur = cur->parent) {
        AVLNode *parent = cur->parent;
        if (parent->right == cur) {
            return parent;
        }
    }
    // it is least value node
    return NULL;
}

// receive node and offset (can be + or -)
// then count the node forward or backward, return the result
// note: if offset is outbound, we return NULL
AVLNode *avl_offset(AVLNode *node, int64_t offset) {
    assert(node);
    // positive case: stop at 0 (don't get into next loop)
    for (; offset > 0 && node != NULL; offset--) {
        node = successor(node);
    }
    // negative case 
    for (; offset < 0 && node != NULL; offset++) {
        node = predecessor(node);
    }
    // if offset = 0, just fall through until here
    return node; 
}
