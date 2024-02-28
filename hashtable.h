/**
 * a chaining hashtable
 * 
*/
#include <stddef.h>
#include <stdint.h>

struct HNode{
    HNode *next = NULL;
    size_t hashcode;
};

struct Hashtable{
    HNode **tab = NULL;
    size_t mask = 0;
    size_t size = 0;  
};

// the user-see hashmap
struct Hashmap{
    Hashtable ht1;
    Hashtable ht2;
    size_t rp = 0;
};

// n must 2^k
static void h_init(Hashtable* ht,size_t n);

static void h_insert(Hashtable *ht, HNode *node);

// look up hashtable ht , use equl function and return the address of node that has the key
static HNode **h_lookup(Hashtable *ht, HNode *key, bool (*eq)(HNode *, HNode *));

static HNode* h_detach(Hashtable *ht,HNode ** from);

const size_t k_max_load_factor = 8;

// real
void hm_insert(Hashmap *hm, HNode *node);

// double resize the 
void hm_resize_init(Hashmap *hm);

const size_t k_resizing_work = 128;

static void hm_resizing(Hashmap *hmap) ;

HNode *hm_lookup(Hashmap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));

HNode *hm_pop(Hashmap *hm,HNode *key,bool (*eq)(HNode *,HNode *));

void hm_destroy(Hashmap *hmap);

size_t hm_size(Hashmap *hm);

