#include <assert.h>
#include <stdlib.h>
#include "hashtable.h"


// n must be a power of 2
static void h_init(Hashtable *htab, size_t n) {
    assert(n > 0 && ((n - 1) & n) == 0);
    htab->tab = (HNode **)calloc(sizeof(HNode *), n);
    htab->mask = n - 1;
    htab->size = 0;
}

// hashtable insertion
static void h_insert(Hashtable *htab, HNode *node) {
    size_t pos = node->hashcode & htab->mask;
    HNode *next = htab->tab[pos];
    node->next = next;
    htab->tab[pos] = node;
    htab->size++;
}

// hashtable look up subroutine.
// Pay attention to the return value. It returns the address of
// the parent pointer that owns the target node,
// which can be used to delete the target node.
static HNode **h_lookup(Hashtable *htab, HNode *key, bool (*eq)(HNode *, HNode *)) {
    if (!htab->tab) {
        return NULL;
    }
    size_t pos = key->hashcode & htab->mask;
    HNode **from = &htab->tab[pos];     // incoming pointer to the result
    for (HNode *cur; (cur = *from) != NULL; from = &cur->next) {
        if (cur->hashcode == key->hashcode && eq(cur, key)) {
            return from;
        }
    }
    return NULL;
}

// remove a node from the chain
static HNode *h_detach(Hashtable *htab, HNode **from) {
    HNode *node = *from;
    *from = node->next;
    htab->size--;
    return node;
}

static void hm_help_resizing(Hashmap *hmap) {
    size_t nwork = 0;
    while (nwork < k_resizing_work && hmap->ht2.size > 0) {
        // scan for nodes from ht2 and move them to ht1
        HNode **from = &hmap->ht2.tab[hmap->rp];
        if (!*from) {
            hmap->rp++;
            continue;
        }

        h_insert(&hmap->ht1, h_detach(&hmap->ht2, from));
        nwork++;
    }

    if (hmap->ht2.size == 0 && hmap->ht2.tab) {
        // done
        free(hmap->ht2.tab);
        hmap->ht2 = Hashtable{};
    }
}

static void hm_start_resizing(Hashmap *hmap) {
    assert(hmap->ht2.tab == NULL);
    // create a bigger hashtable and swap them
    hmap->ht2 = hmap->ht1;
    h_init(&hmap->ht1, (hmap->ht1.mask + 1) * 2);
    hmap->rp = 0;
}

HNode *hm_lookup(Hashmap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_resizing(hmap);
    HNode **from = h_lookup(&hmap->ht1, key, eq);
    from = from ? from : h_lookup(&hmap->ht2, key, eq);
    return from ? *from : NULL;
}

void hm_insert(Hashmap *hmap, HNode *node) {
    if (!hmap->ht1.tab) {
        h_init(&hmap->ht1, 4);
    }
    h_insert(&hmap->ht1, node);

    if (!hmap->ht2.tab) {
        // check whether we need to resize
        size_t load_factor = hmap->ht1.size / (hmap->ht1.mask + 1);
        if (load_factor >= k_max_load_factor) {
            hm_start_resizing(hmap);
        }
    }
    hm_help_resizing(hmap);
}

HNode *hm_pop(Hashmap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_resizing(hmap);
    if (HNode **from = h_lookup(&hmap->ht1, key, eq)) {
        return h_detach(&hmap->ht1, from);
    }
    if (HNode **from = h_lookup(&hmap->ht2, key, eq)) {
        return h_detach(&hmap->ht2, from);
    }
    return NULL;
}

size_t hm_size(Hashmap *hmap) {
    return hmap->ht1.size + hmap->ht2.size;
}

void hm_destroy(Hashmap *hmap) {
    free(hmap->ht1.tab);
    free(hmap->ht2.tab);
    *hmap = Hashmap{};
}