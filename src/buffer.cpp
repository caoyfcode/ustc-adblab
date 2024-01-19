#include "buffer.h"
#include <cstring>
#include <iostream>

Frame buf[DEFBUFSIZE];

BufferManager::BufferManager(DataStorageManager *dsmgr, Replacer::Algo algo)
{
    memset(m_ftop, 0xffff, DEFBUFSIZE * sizeof(int)); // 全部设置为 -1
    memset(m_ptof, 0, DEFBUFSIZE * sizeof(BCB *)); // 全部设置为 nullptr
    m_dsmgr = dsmgr;
    m_replacer = Replacer::create(algo);
    access_count = hit_count = 0;
}

// 得到 page 对应的 frame_id, 可以认为是 requestor 在访问一次某 page
int BufferManager::fix_page(int page_id, bool write)
{
    access_count++;
    int hashed_page_id = hash(page_id);
    for (BCB *p = m_ptof[hashed_page_id]; p != nullptr; p = p->next) {
        if (p->page_id == page_id) {
            hit_count++;
            p->count++;
            m_replacer->access_frame(p, write);
            if (write) {
                p->dirty = 1;
            }
            return p->frame_id;
        }
    }
    BCB *bcb = select_victim();
    bcb->page_id = page_id;
    bcb->count = 1;
    bcb->dirty = 0;
    bcb->next = nullptr;
    int frame_id = bcb->frame_id;
    m_dsmgr->read_page(page_id, buf[frame_id].field);
    if (m_ptof[hashed_page_id] != nullptr) {
        bcb->next = m_ptof[hashed_page_id];
        m_ptof[hashed_page_id] = bcb;
    } else {
        bcb->next = nullptr;
        m_ptof[hashed_page_id] = bcb;
    }
    m_ftop[frame_id] = page_id;
    m_replacer->insert_bcb(bcb, write);
    if (write) {
        bcb->dirty = 1;
    }
    return frame_id;
}

// 创建新 page, 得到新 page 的 page_id 与 frame_id, 可以认为同时也写了此 page
PageFrame BufferManager::fix_new_page()
{
    access_count++;
    int page_id = -1, frame_id;
    for (int i = 0; i < m_dsmgr->get_num_pages(); i++) {
        if (m_dsmgr->get_use(i) == 0) {
            page_id = i;
            break;
        }
    }
    if (page_id == -1) { // 未找到未使用的 page
        page_id = m_dsmgr->get_num_pages();
        m_dsmgr->inc_num_pages();
    }
    m_dsmgr->set_use(page_id, 1);
    frame_id = fix_page(page_id, 1); // 新页一定要写的, 故为 1
    return {page_id, frame_id};
}

// Requestor unpin a frame, 但是由于认为 fix_page 是一次访问, 所以在那时设置了 dirty
int BufferManager::unfix_page(int page_id)
{
    int hashed_page_id = hash(page_id);
    for (BCB *p = m_ptof[hashed_page_id]; p != nullptr; p = p->next) {
        if (p->page_id == page_id) {
            p->count--;
            return p->frame_id;
        }
    }
    return -1;
}

int BufferManager::num_free_frames()
{
    int count = 0;
    for (int i = 0; i < DEFBUFSIZE; i++) {
        if (m_ftop[i] < 0) {
            count++;
        }
    }
    return count;
}

// 首先寻找有没有空闲的 frame, 如果没有就调用替换算法进行 select_victim, 并进行换出操作, 同时构造新的或者复用旧的 BCB
BCB *BufferManager::select_victim()
{
    for (int i = 0; i < DEFBUFSIZE; i++) {
        if (m_ftop[i] < 0) {
            return new BCB {-1, i};
        }
    }
    // 未找到, 则调用替换算法找到被替换的 frame, 并替换
    BCB *bcb = m_replacer->select_victim();
    int frame_id = bcb->frame_id;
    m_replacer->remove_bcb(bcb);
    if (bcb->dirty) {
        m_dsmgr->write_page(bcb->page_id, buf[frame_id].field);
    }
    // BCB 结构体从哈希表中取出
    int hashed_page_id = hash(bcb->page_id);
    if (bcb == m_ptof[hashed_page_id]) {
        m_ptof[hashed_page_id] = bcb->next;
    } else {
        BCB *pre;
        for (pre = m_ptof[hashed_page_id]; pre->next != bcb; pre = pre->next);
        pre->next = bcb->next;
    }
    m_ftop[frame_id] = -1;
    return bcb;
}

int BufferManager::hash(int page_id)
{
    return page_id % DEFBUFSIZE;
}

void BufferManager::set_dirty(int frame_id)
{
    int page_id = m_ftop[frame_id];
    int hashed_page_id = hash(page_id);
    for (BCB *p = m_ptof[hashed_page_id]; p != nullptr; p = p->next) {
        if (p->frame_id == frame_id) {
            p->dirty = 1;
            return;
        }
    }
}

void BufferManager::unset_dirty(int frame_id)
{
    int page_id = m_ftop[frame_id];
    int hashed_page_id = hash(page_id);
    for (BCB *p = m_ptof[hashed_page_id]; p != nullptr; p = p->next) {
        if (p->frame_id == frame_id) {
            p->dirty = 0;
            return;
        }
    }
}

void BufferManager::write_dirtys()
{
    for (int i = 0; i < DEFBUFSIZE; i++) {
        for (BCB* p = m_ptof[i]; p != nullptr; p = p->next) {
            p->dirty = 1;
        }
    }
}

void BufferManager::print_frame(int frame_id)
{
    std::cout << buf[frame_id].field << std::endl;
}

BufferManager::~BufferManager()
{
    for (int i = 0; i < DEFBUFSIZE; i++) {
        for (BCB* p = m_ptof[i]; p != nullptr;) {
            if (p->dirty) {
                m_dsmgr->write_page(p->page_id, buf[p->frame_id].field);
            }
            BCB *tmp = p;
            p = p->next;
            delete tmp;
        }
    }
    delete m_replacer;
}