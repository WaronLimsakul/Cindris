#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <set>

#include "avltree.h"
#include "common.h"

// Generate test cases with code.
// Verify data structure properties and invariants.
// Compare to a reference data structure such as std::set.

struct Data {
    AVLNode node;
    uint32_t data;
};

struct Tree {
    AVLNode *root = NULL;
};

// cons: add new node to a tree
// 1. malloc new data
// 2. assign value
// 3. get a node from new data
// 4. find a place for a node to live (bst search)
//  - if already, go right, we allow duplicate value
// 5. assign value 
// 6. fix start by inserted node
static void tree_add(Tree &t, uint32_t val) {
    Data *new_data = new Data{}; 
    new_data->data = val;
    avl_init(&new_data->node);
    
    AVLNode **from = &t.root;
    AVLNode *cur = NULL;
    while (*from) {
        cur = *from;
        uint32_t cur_data = container_of(cur, Data, node)->data;
        from = cur_data > val ? &cur->left : &cur->right;
    }
    *from = &new_data->node;
    new_data->node.parent = cur;
    t.root = avl_fix(&new_data->node);
}

// cons: delete target value from a tree
// plan
// 1. traverse tree to find node with that value
// 2. if not found, return
// 3. if found, set new root after avl_del(node)
// 4. then delete the entire data we detach
static bool tree_del(Tree &t, uint32_t val) { 
    AVLNode *cur = t.root;     
    while (cur != NULL) {
        uint32_t cur_val = container_of(cur, Data, node)->data;
        if (cur_val == val) break;
        cur = cur_val > val ? cur->left : cur->right;
    }
    if (!cur) return false;
    t.root = avl_del(cur);
    delete container_of(cur, Data, node);
    return true;
}

static uint32_t absolute(int32_t val) {
    return val > 0 ? val : -val;
}

// cons: verify that parent and node align properly
// plan:
// 1. check if parent and node are parent-child
// 2. verify node and its left + right child
// 3. assert node count and height according to its children
// 4. assert its avl property
// 5. assert left child and right child's parent are node
// 6. assert value left < node < right
static void avl_verify(AVLNode *parent, AVLNode *node) {
    if (!node) return;
    assert(node->parent == parent);    

    assert(avl_count(node) == 
           avl_count(node->left) + 
           avl_count(node->right) + 1);

    uint32_t hleft = avl_height(node->left);
    uint32_t hright = avl_height(node->right);
    assert(avl_height(node) == max(hleft, hright) + 1);
    
    assert(absolute(hleft - hright) <= 1);

    uint32_t node_data = container_of(node, Data, node)->data;
    if (node->left) {
        assert(node->left->parent == node);
        uint32_t left_data = container_of(node->left, Data, node)->data;
        assert(left_data <= node_data);
    }
    if (node->right) {
        assert(node->right->parent == node);
        uint32_t right_data = container_of(node->right, Data, node)->data;
        assert(right_data >= node_data);
    }
}

// cons: extract entire subtree's value start with node to the multiset
// plan:
// - check edge case: node = NULL
// - extract left
// - put node value in to extract
// - extract 
static void extract(AVLNode *node, std::multiset<uint32_t> &extracted) {
    if (node == NULL) return;
    extract(node->left, extracted);
    extracted.insert(container_of(node, Data, node)->data);
    extract(node->right, extracted);
}

// cons: verify if the tree is effectively equal to the ref
static void tree_verify(Tree &t, const std::multiset<uint32_t> &ref) {
    avl_verify(NULL, t.root); 
    if (t.root) {
        assert(t.root->count == ref.size());
    }
    std::multiset<uint32_t> extracted;
    extract(t.root, extracted);
    assert(extracted == ref);
}

// cons: free all the Data that we malloc in the tree
static void tree_destroy(Tree &t) {
    while (t.root) {
        AVLNode *root = t.root;
        t.root = avl_del(root);
        delete container_of(root, Data, node);
    }
}

// cons: test for every number from 0 to size - 1
// 1. we will add range(0, size) except that number as a tree and multiset
// 2. then verify these 2
// 3. then add that number to tree, insert that to multiset
// 4. then verify them again
static void test_insert(uint32_t size) {
    for (uint32_t target = 0; target < size; target++) {
        Tree tree;
        std::multiset<uint32_t> ref;
        for (uint32_t i = 0; i < size; i++) {
            if (i == target) continue;
            tree_add(tree, i);
            ref.insert(i);
        }
        tree_verify(tree, ref);

        tree_add(tree, target);
        ref.insert(target);
        tree_verify(tree, ref);
        tree_destroy(tree);
    } 
}

// same thing as before but we will not skip the target
// we will test if we can use duplicate value with this tree
static void test_insert_dup(uint32_t size) {
    for (uint32_t target = 0; target < size; target++) {
        Tree tree;
        std::multiset<uint32_t> ref;
        for (uint32_t i = 0; i < size; i++) {
            tree_add(tree, i);
            ref.insert(i);
        }
        tree_verify(tree, ref);

        tree_add(tree, target);
        ref.insert(target);
        tree_verify(tree, ref);
        tree_destroy(tree);
    } 
}

// 0. for each target from range(0, size)
// 1. add range(0, size) to the tree and a multiset
// 2. verify them
// 3. delete target from both
// 4. verify them again and destroy the tree
static void test_del(uint32_t size) {
    for (uint32_t target = 0; target < size; target++) {
        Tree tree;
        std::multiset<uint32_t> ref;
        for (uint32_t i = 0; i < size; i++) {
            tree_add(tree, i);
            ref.insert(i);
        }
        tree_verify(tree, ref);

        // make sure it delete something
        assert(tree_del(tree, target));
        ref.erase(target);
        tree_verify(tree, ref);
        tree_destroy(tree);
    }
}

// Arrange-Act-Assert
int main(void) {
    Tree tree = Tree{};
    
    // stage 1: basic test
    tree_verify(tree, {}); // quick way to init empty container
    tree_add(tree, 123);
    tree_verify(tree, {123});
    assert(!tree_del(tree, 124));
    assert(tree_del(tree, 123));
    tree_verify(tree, {}); // quick way to init empty container

    // stage 2: sequential insertion verify
    std::multiset<uint32_t> ref;
    for (int i = 0; i < 1000; i += 3) {
        tree_add(tree, i);
        ref.insert(i);
        tree_verify(tree, ref);
    }

    // stage 3: random insert 
    for (int i = 0; i < 100; i++) {
        uint32_t random_val = (uint32_t)rand() % 1000;
        tree_add(tree, random_val);
        ref.insert(random_val);
        tree_verify(tree, ref);
    }

    // stage 4: random delete
    for (int i = 0; i < 100; i++) {
        uint32_t random_val = (uint32_t)rand() % 1000;
        auto iter = ref.find(random_val);
        if (iter == ref.end()) {
            // if ref doesn't found, we shouldn't either
            assert(!tree_del(tree, random_val));
        } else {
            assert(tree_del(tree, random_val));
            // if put random_val, it will erase every random_val in it
            ref.erase(iter); 
        }
        tree_verify(tree, ref);
    }

    
    // stage 5: insert/delete at various position
    for (int i = 0; i < 200; i++) {
        test_insert(i);
        test_insert_dup(i);
        test_del(i);
    }

    tree_destroy(tree);
}

