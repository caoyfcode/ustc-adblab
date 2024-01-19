#pragma once
#include <string>
#include <cstdio>

#define PAGESIZE 4096
#define MAXPAGES 60000

class DataStorageManager {
public:
    DataStorageManager();
    int open_file(std::string filename);
    int close_file();
    int read_page(int page_id, char *frame);
    int write_page(int page_id, const char *frame);
    int seek(int offset, int pos); // 实现但未使用
    FILE *get_file();
    void inc_num_pages();
    int get_num_pages();
    void set_use(int index, int use_bit);
    int get_use(int index);
    int io_count;
private:
    FILE *m_curr_file;
    int m_num_pages;
    int m_pages[MAXPAGES];
};