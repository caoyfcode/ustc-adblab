#include <cstdlib>
#include <ctime>
#include "buffer.h"

class LinkedList {
public:
    BCB *head, *tail;
    LinkedList(): head(nullptr), tail(nullptr) {}
    void insert_tail(BCB *bcb) {
        bcb->algo_next = nullptr;
        if (tail != nullptr) {
            bcb->algo_prev = tail;
            tail->algo_next = bcb;
            tail = bcb;
        } else {
            bcb->algo_prev = nullptr;
            head = tail = bcb;
        }
    }
    void remove(BCB *bcb) {
        if (bcb->algo_prev && bcb->algo_next) {
            bcb->algo_next->algo_prev = bcb->algo_prev;
            bcb->algo_prev->algo_next = bcb->algo_next;
        } else if (bcb->algo_prev && !bcb->algo_next) {
            bcb->algo_prev->algo_next = nullptr;
            tail = bcb->algo_prev;
        } else if (!bcb->algo_prev && bcb->algo_next) {
            bcb->algo_next->algo_prev = nullptr;
            head = bcb->algo_next;
        } else {
            head = tail = nullptr;
        }
        bcb->algo_next = bcb->algo_prev = nullptr;
    }
};

// 这里实现的替换算法由于没有实现并发, 因而基于所有的 pin-count 都为 0 来实现

class LruReplacer: public Replacer {
public:
    LruReplacer(): list() {}
    ~LruReplacer() override {}
    Algo get_algo() const override {
        return Algo::LRU;
    }
    void access_frame(BCB *bcb, bool write) override {
        list.remove(bcb);
        list.insert_tail(bcb);
    }
    void remove_bcb(BCB *bcb) override {
        list.remove(bcb);
    }
    void insert_bcb(BCB *bcb, bool write) override {
        list.insert_tail(bcb);
    }
    BCB *select_victim() const override {
        return list.head;
    }
protected:
    LinkedList list; // head is lru, tail is mru
};

class MruReplacer: public LruReplacer {
public:
    Algo get_algo() const override {
        return Algo::MRU;
    }
    BCB *select_victim() const override {
        return list.tail;
    }
};

class RandomReplacer: public Replacer {
public:
    RandomReplacer() {
        srand((unsigned int)time(nullptr));
    }
    ~RandomReplacer() override {}
    Algo get_algo() const override {
        return Algo::RANDOM;
    }
    void access_frame(BCB *bcb, bool write) override {}
    void remove_bcb(BCB *bcb) override {
        m_frame_table[bcb->frame_id] = nullptr;
    }
    void insert_bcb(BCB *bcb, bool write) override {
        m_frame_table[bcb->frame_id] = bcb;
    }
    BCB *select_victim() const override {
        int frame_id = (int)((double)rand() / RAND_MAX * DEFBUFSIZE) % DEFBUFSIZE;
        return m_frame_table[frame_id];
    }
private:
    // std::mt19937 m_gen;
    BCB *m_frame_table[DEFBUFSIZE] = {0};
};

/**
 * 该算法用数组作环, 依赖于 buffer_manager 的行为: 
 * 1. 空闲 frame 总是最后一个非空闲的下一个, 换言之, 每次加一
 * 2. 当从缓存中换出一页时, 不释放旧的 BCB 的内存, 不构造新的 BCB, 让换入的页复用旧 BCB 内存
 * 因而,在实现时,
 * 1. 以负方向为 current 递增方向(因为正方向是刚访问的), 并且任何 BCB 从不移除出环
 * 2. 当 remove_bcb 调用时什么也不做, insert_bcb 调用可以通过让环的大小加一实现
*/
class ClockReplacer: public Replacer {
public:
    ClockReplacer(): ring() {}
    ~ClockReplacer() override {}
    Algo get_algo() const override {
        return Algo::CLOCK;
    }
    void access_frame(BCB *bcb, bool write) override {
        bcb->referenced = 1;
    }
    // Clock 比其它的特殊, 其被换出也不离开环
    void remove_bcb(BCB *bcb) override { /* do nothing */ }
    void insert_bcb(BCB *bcb, bool write) override {
        bcb->referenced = 1;
        // 空闲 frame 总是最后一个非空闲的下一个
        if (bcb->frame_id == ring_length) {
            ring[ring_length] = bcb;
            ring_length++;
        }
    }
    BCB *select_victim() const override {
        BCB *victim = nullptr;
        while (!victim) {
            if (ring[current]->count > 0) {
                current = (current + ring_length - 1) % ring_length; // 以负数方向为正方向
            } else if (ring[current]->referenced == 1) {
                ring[current]->referenced = 0;
                current = (current + ring_length - 1) % ring_length;
            } else {
                victim = ring[current];
                current = (current + ring_length - 1) % ring_length;
            }
        }
        
        return victim;
    }
private:
    // 用数组作环, 并且因为新的空闲的是非空闲的下一个, 所以以负数方向为正方向
    mutable BCB *ring[DEFBUFSIZE]= {0}; 
    mutable int current;
    int ring_length;
};

class Lru2Replacer: public Replacer {
public:
    Lru2Replacer(): time(0), lru(), sorted() {}
    ~Lru2Replacer() override {}
    Algo get_algo() const override {
        return Algo::LRU_2;
    }
    void access_frame(BCB *bcb, bool write) override {
        time++;
        if (bcb->time[1] == 0) { // 访问过 1 次
            lru.remove(bcb);
        } else { // 访问过 2 次
            sorted.remove(bcb);
        }
        bcb->time[1] = bcb->time[0];
        bcb->time[0] = time;
        // insert bcb into sorted queue
        if (sorted.head == nullptr) {
            sorted.head = sorted.tail = bcb;
            bcb->algo_next = bcb->algo_prev = nullptr;
        } else {
            if (sorted.head->time[1] > bcb->time[1]) {
                bcb->algo_next = sorted.head;
                bcb->algo_prev = nullptr;
                sorted.head->algo_prev = bcb;
                sorted.head = bcb;
            } else {
                BCB *p = sorted.head;
                while (p->algo_next && p->algo_next->time[1] <= bcb->time[1]) {
                    p = p->algo_next;
                }
                if (p->algo_next) {
                    p->algo_next->algo_prev = bcb;
                }
                bcb->algo_next = p->algo_next;
                bcb->algo_prev = p;
                p->algo_next = bcb;
            }
        }
    }
    void remove_bcb(BCB *bcb) override {
        if (bcb == lru.head) {
            lru.remove(bcb);
        } else {
            sorted.remove(bcb);
        }
    }
    void insert_bcb(BCB *bcb, bool write) override {
        time++;
        bcb->time[1] = 0;
        bcb->time[0] = time;
        lru.insert_tail(bcb);
    }
    BCB *select_victim() const override {
        if (lru.head) { // 首先淘汰少于 k 次的
            return lru.head;
        } else {
            return sorted.head;
        }
    }
private:
    LinkedList lru; // 左侧访问次数小于 2 的 LRU 链表
    // TODO 修改为小顶堆实现
    LinkedList sorted; // 右侧访问次数不少于 2 的按照倒数第 2 时间的 FIFO 链表, 队头为最旧的, 优先出队
    int time; // time 初始为 0, 在 BCB 内最小为 1, 如果出现了一个 0, 说明只访问过 1 次
};
/*
lru-2:
    io count: 419835
    access count: 500000
    hit count: 217857
    hit rate: 0.435714
    time: 45.1056s
*/

class TwoQueueReplacer: public Replacer {
public:
    TwoQueueReplacer(): fifo(), lru() {}
    ~TwoQueueReplacer() override {}
    Algo get_algo() const override {
        return Algo::TWO_QUEUE;
    }
    void access_frame(BCB *bcb, bool write) override {
        if (bcb->access_times == 1) {
            fifo.remove(bcb);
        } else {
            lru.remove(bcb);
        }
        bcb->access_times = 2;
        lru.insert_tail(bcb);
    }
    void remove_bcb(BCB *bcb) override {
        if (bcb->access_times == 1) {
            fifo.remove(bcb);
        } else {
            lru.remove(bcb);
        }
    }
    void insert_bcb(BCB *bcb, bool write) override {
        bcb->access_times = 1;
        fifo.insert_tail(bcb);
    }
    BCB *select_victim() const override {
        if (fifo.head) {
            return fifo.head;
        } else {
            return lru.head;
        }
    }
private:
    LinkedList fifo; // 只访问过 1 次的 fifo
    LinkedList lru; // 访问过 2 次及以上的 lru
};

Replacer *Replacer::create(Algo algo)
{
    switch (algo) {
        case LRU:
            return new LruReplacer;
        case MRU:
            return new MruReplacer;
        case RANDOM:
            return new RandomReplacer;
        case CLOCK:
            return new ClockReplacer;
        case LRU_2:
            return new Lru2Replacer;
        case TWO_QUEUE:
            return new TwoQueueReplacer;
        default:
            return new LruReplacer;
    }
}