#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <vector>

#include "Node.h"
#include "skiplist.h"
#include "hashtable.h"

#ifndef NDEBUG
#   define toydies_assert(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << ": " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)
#else
#   define ASSERT(condition, message) do { } while (false)
#endif

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})

extern const size_t k_max_msg;

static uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

struct Entry{
    struct HNode node;
    std::string k;
    std::string v;
};

static struct kv{
    Hashmap db;
    SkipList<int,std::string> sl;
    kv():sl(0x7fffffff){};
}g_data;

static bool entry_eq(HNode *lhs, HNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->k == re->k;
}


//-----------handle part--------
//----------Serialization------part
enum DATA_TYPE{
    SER_NIL = 0,    // Like `NULL`
    SER_ERR = 1,    // An error code and message
    SER_STR = 2,    // A string
    SER_INT = 3,    // A int64
    SER_LIST = 4,   // an (k - v)array in range search
};

// --- warpper
static void out_null(std::string &out) {
    out.push_back(SER_NIL);
}

static void out_str(std::string &out, const std::string &val) {
    out.push_back(SER_STR);
    uint32_t len = (uint32_t)val.size();
    out.append((char *)&len, 4);
    out.append(val);
}

static void out_int(std::string &out, int64_t val) {
    out.push_back(SER_INT);
    out.append((char *)&val, 8);
}

static void out_err(std::string &out, int32_t code, const std::string &msg) {
    out.push_back(SER_ERR);
    out.append((char *)&code, 4);
    uint32_t len = (uint32_t)msg.size();
    out.append((char *)&len, 4);
    out.append(msg);
}

static void out_search(std::string &out, std::vector<Node<int, std::string> *> &ans){
    out.push_back(SER_LIST);
    // flag | size | each of them 
    uint32_t nums = (uint32_t)(2 * ans.size());
    toydies_assert(nums % 2 == 0 ,"error in kv nums");
    out.append((char *)&nums, 4);
    for(auto x : ans){
        auto v = x->getKey();
        // string k
        auto k = x->getValue();
        // int value
        std::cout << "k :" << k << "\n";
        std::cout << "v :" << v << "\n";
        out_str(out,k);
        out_int(out,v);
    }
}

static void do_get(std::vector<std::string> &cmd, std::string &out){
    Entry entry;
    entry.k.swap(cmd[1]);
    entry.node.hashcode = str_hash((uint8_t *)entry.k.data(), entry.k.size());
    HNode *node = hm_lookup(&g_data.db, &entry.node, &entry_eq);
    if (!node) {
        return  out_null(out);
    }
    const std::string &val = container_of(node, Entry, node)->v;
    assert(val.size() <= k_max_msg);
    out_str(out, val);
}


//TODO: add skiplist feature
static void do_set(std::vector<std::string> &cmd, std::string &out){
    Entry entry;

    // set k v
    // store in (int)v k
    auto k = std::stoi(cmd[2]);
    // string to int

    // key
    auto v = cmd[1];
    g_data.sl.insert(k,v);
    
    entry.k.swap(cmd[1]);
    entry.node.hashcode = str_hash((uint8_t *)entry.k.data(), entry.k.size());
    auto node = hm_lookup(&g_data.db,&entry.node,&entry_eq);
    if(node){
        // 如果已经有k了，那么修改这个k对应的值
        container_of(node,Entry,node)->v.swap(cmd[2]);
    }else{
        //如果没有k，则进行初始化,必须new一块heap上的内存，不能declear临时变量
        Entry *ent = new Entry();
        ent->k.swap(entry.k);
        ent->node.hashcode = entry.node.hashcode;
        ent->v.swap(cmd[2]);
        hm_insert(&g_data.db, &ent->node);
    }
    return out_null(out);
}


//TODO: support skip list
static void do_del(std::vector<std::string> &cmd, std::string &out){
    Entry entry;
    entry.k.swap(cmd[1]);
    //TODO:如何根据k找到v
    entry.node.hashcode = str_hash((uint8_t *)entry.k.data(), entry.k.size());
    HNode *node = hm_pop(&g_data.db, &entry.node, &entry_eq);

    const std::string &val = container_of(node, Entry, node)->v;

    auto sl_k = std::stoi(val);

    auto v = g_data.sl.search(sl_k)->getValue();

    g_data.sl.remove(sl_k,v);


    if (node) {
        delete container_of(node, Entry, node);
    }
    return out_int(out, node ? 1 : 0);;
}

//TODO:
static void do_search(std::vector<std::string> &cmd,std::string &out){
    // search k2 k2 
    auto k1 = stoi(cmd[1]);
    auto k2 = stoi(cmd[2]);
    auto vec = g_data.sl.range_search(k1,k2);

    // list of string between the 
    out_search(out,vec);
}
