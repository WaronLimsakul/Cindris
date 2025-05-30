#pragma once
#include <stdint.h>
#include <string>

// for intrusive node
#define container_of(ptr, T, field) \
    (T *)((char *)ptr - offsetof(T, field))

struct HNode {
    struct HNode *next = NULL;
    uint64_t hashval = 0;
};

struct HTable {
    HNode **table = NULL; // array of slot (HNode *)
    size_t mask = 0; // choose N = 2^n, because it's the "% 2^n" is just "& 2^n - 1"
    size_t size = 0; // fixed size
};

// in normal situation: we use new
struct HMap {
    HTable older;
    HTable newer;
    size_t migrate_pos = 0;
};

struct Cache {
    HMap map;
};

struct Entry {
    std::string key;
    std::string value;
    HNode node;
};


// interface when doing get, set, del
HNode *hm_lookup(HMap *map, HNode *node, bool (* eq)(HNode *, HNode *));
void hm_insert(HMap *map, HNode *node);
HNode *hm_delete(HMap *map, HNode *node, bool (* eq)(HNode *, HNode *));
bool entry_eq(HNode *n1, HNode *n2);

