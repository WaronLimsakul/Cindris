#include <assert.h>
#include <stdlib.h>

#include "hashtable.h"
#include "common.h"


// chaining hash table allow > 1 load factor
const size_t max_load_factor = 4;
const size_t rehash_work = 128;

// - The key of this hash is we will have `offset_basis` and `prime`
// - for every byte in our string, `xor` it with `acc` , which start with `basis`
// - then `*=` with `prime` 
// - search google for `basis` and `prime` for FNV hash 
// - return `acc` (whihc is our hash)
uint64_t str_hash(uint8_t *data, size_t len) {
    uint64_t hash = 0xcbf29ce484222325; // start with offset basis
    const uint64_t prime = 0x00000100000001b3;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i]; 
        hash *= prime;
    }
    return hash;
}

static void h_init(HTable *htab, size_t num_slots) {
    // check n = 2^k
    assert(num_slots > 0 && ((num_slots & (num_slots - 1)) == 0));
    // array of linked list
    htab->table = (HNode **)calloc(num_slots, sizeof(HNode *));
    htab->mask = num_slots - 1; // will do "& mask"
    htab->size = 0;
}

// just insert into slot
static void h_insert(HTable *htab, HNode *node) {
    uint64_t slot_idx = node->hashval & htab->mask;
    HNode *head = htab->table[slot_idx];
    node->next = head;
    htab->table[slot_idx] = node;
    htab->size++;
}

// 1. use the hashval of the target to go to target slot
// 2. traverse the chain and compare using eq(cur, target)
static HNode **h_lookup(HTable *htab, HNode *target, bool (* eq)(HNode *, HNode *)) {
    if (!htab || !htab->table) return NULL;

    uint64_t slot_idx = target->hashval & htab->mask;
    HNode **cur_ptr = &htab->table[slot_idx];
    HNode *cur;
    // need to return address of target pointer, so we can delete too
    for (; (cur = *cur_ptr) != NULL; cur_ptr = &(cur->next)) {
        if (cur->hashval == target->hashval && eq(cur, target)) {
            return cur_ptr;
        }
    }
    return NULL;
}

// like doing prev->next = prev->next->next and return the detached node*
static HNode *h_detach(HTable *htab, HNode **target_ptr) {
    HNode *node = *target_ptr;
    *target_ptr = node->next;
    htab->size--;
    return node;
}

static void hm_rehash_init(HMap *map) {
    if (map->older.table != NULL) return;
    map->older = map->newer;
    h_init(&map->newer, (map->older.mask + 1) * 2); // resize to 2^(n+1)
    map->migrate_pos = 0;
}

HNode *hm_lookup(HMap *map, HNode *target, bool (* eq)(HNode *, HNode *)) {
    if (!map) return NULL;
    HNode **res = h_lookup(&map->newer, target, eq);
    if (res == NULL) {
        res = h_lookup(&map->older, target, eq);
    }
    return res != NULL ? *res : NULL;
}

// detach the target node (can be dummy) from map and return detached node 
HNode *hm_delete(HMap *map, HNode *target, bool(* eq)(HNode *, HNode *)) {
    HNode **target_ptr = h_lookup(&map->newer, target, eq);
    if ((target_ptr == h_lookup(&map->newer, target, eq)) != NULL) {
        return h_detach(&map->newer, target_ptr);
    }
    if ((target_ptr = h_lookup(&map->older, target, eq)) != NULL) {
        return h_detach(&map->older, target_ptr);
    }
    return NULL;
}

// plan
// 1. for loop 10 (another cond: `migrate_pos` != `mask + 1`)
// 2. run migrate_pos until we found something (or it runs out, break)
// 3. at `migrate_pos` idx linked list, detach the head get node pointer
// 4. then put it in new one
// 5. check at the end if we done rehashing
static void hm_help_rehashing(HMap *map) {
    if (!map->older.table) return;
    size_t slots = map->older.mask + 1;
    for (size_t moved = 0; moved < rehash_work && map->migrate_pos != slots; moved++) {
        // find the non-empty slot
        while (map->migrate_pos != slots && map->older.table[map->migrate_pos] == NULL) {
            map->migrate_pos++;
        }
        // break if we done
        if (map->migrate_pos == slots) break;
        HNode **head_ptr = &map->older.table[map->migrate_pos];
        HNode *detached_head = h_detach(&map->older, head_ptr);
        h_insert(&map->newer, detached_head);
    }

    // clear after done rehashing
    if (map->older.size == 0 && map->older.table) {
        free(map->older.table);
        map->older = HTable{};
    }
}

void hm_insert(HMap *map, HNode *node) {
    if (!map->newer.table) {
        h_init(&map->newer, 4);
    }

    // no point inserting to older one
    h_insert(&map->newer, node);

    if (!map->older.table) {
        // check if current load factor exceed our threshold
        size_t max_keys = (map->newer.mask + 1) * max_load_factor;
        if (map->newer.size >= max_keys) {
            hm_rehash_init(map);
        }
    }
    hm_help_rehashing(map); // move some keys
}

// only compare key
bool entry_eq(HNode *n1, HNode *n2) {
    Entry *e1 = container_of(n1, Entry, node);
    Entry *e2 = container_of(n2, Entry, node);
    return e1->key == e2->key;
}

static size_t h_size(HTable *htab) {
    if (htab->table == NULL) return 0; 
    return htab->size;
}
    
size_t hm_size(HMap *map) {
    return h_size(&map->newer) + h_size(&map->older); 
}

static bool h_foreach(HTable *htab, bool (* fn)(HNode *, void *), void *arg) {
    if (htab->table == NULL) return false;
    size_t slots = htab->mask + 1;
    assert(slots > 0);

    for (size_t i = 0; i < slots; i++) {
        for (HNode *cur = htab->table[i]; cur != NULL; cur = cur->next) {
            if (!fn(cur, arg)) return false;
        }
    }
    return true;
}

// do the function
void hm_foreach(HMap *map, bool (* fn)(HNode *, void *), void *arg) {
    // loop new table, if fail, don't bother the old one
    h_foreach(&map->newer, fn, arg) && h_foreach(&map->older, fn, arg);  
}
