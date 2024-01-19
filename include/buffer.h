#pragma once

#include "data_storage.h"

#define FRAMESIZE 4096
#define DEFBUFSIZE 1024

struct Frame {
    char field[FRAMESIZE];
};

struct BCB
{
    BCB(int page_id, int frame_id): page_id(page_id), frame_id(frame_id), latch(0), count(0), dirty(0), next(nullptr) {};
    int page_id;
    int frame_id;
    int latch;
    int count;
    int dirty;
    BCB *next;
    // 以下为替换算法使用
    BCB *algo_next; // 双向链表
    BCB *algo_prev;
    union {
        int time[2]; // LRU-2 使用的时间(用访问次数当作时间), 0 为倒数第 1 次时间, 1 为倒数第 2 次时间
        int referenced; // CLOCK 使用
        int access_times; // 2Q 使用, 访问过 0 次, 1 次, 2 次
    };
};

struct PageFrame {
    int page_id;
    int frame_id;
};

/**
 * Replacement Algorithm
 * 
 * 替换算法借助 BCB 来实现, 但不拥有 BCB 的所有权
*/
class Replacer {
public:
    enum Algo {LRU, MRU, RANDOM, CLOCK, LRU_2, TWO_QUEUE};
    static Replacer *create(Algo algo);
    virtual Algo get_algo() const = 0;
    virtual ~Replacer() {};
    // 当访问缓存中某 frame 时调用调用
    virtual void access_frame(BCB *bcb, bool write) = 0;
    // 当某 frame 被替换出时调用
    virtual void remove_bcb(BCB *bcb) = 0;
    // 当某空 frame 关联新的 page 后调用, 并且算作一次 access
    virtual void insert_bcb(BCB *bcb, bool write) = 0;
    virtual BCB *select_victim() const = 0;
};

class BufferManager {
public:
    BufferManager(DataStorageManager *dsmgr, Replacer::Algo algo);
    // Interface fucntions
    int fix_page(int page_id, bool write); // 0 for read, 1 for write
    PageFrame fix_new_page();
    int unfix_page(int page_id);
    int num_free_frames();
    ~BufferManager();
    int access_count;
    int hit_count;
private:
    // Internal Functions
    BCB *select_victim();
    int hash(int page_id);
    // void remove_bcb(BCB *ptr, int page_id); // 功能在 select_victim 内了
    // void remove_lru_file(int frid); // 功能在 replacer 实现
    void set_dirty(int frame_id);
    void unset_dirty(int frame_id);
    void write_dirtys();
    void print_frame(int frame_id);
    // Hash Table
    int m_ftop[DEFBUFSIZE]; // frame_id 作为 index, 得到 page_id
    BCB *m_ptof[DEFBUFSIZE]; // hash(page_id) 作为 index, 得到所在的 BCB 溢出链表
    DataStorageManager *m_dsmgr;
    Replacer *m_replacer;
};
