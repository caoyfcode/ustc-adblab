#include <cstring>
#include "data_storage.h"

static const char page_default_content[PAGESIZE] = {0};

DataStorageManager::DataStorageManager(): m_curr_file(nullptr), m_num_pages(0), io_count(0)
{
    memset(m_pages, 0, MAXPAGES * sizeof(int));
}


int DataStorageManager::open_file(std::string filename)
{
    m_curr_file = fopen(filename.c_str(), "r+b");
    if (!m_curr_file) {
        m_curr_file = fopen(filename.c_str(), "w+b");
    }
    fseek(m_curr_file, 0, SEEK_END);
    auto length = ftell(m_curr_file);
    m_num_pages = length / PAGESIZE;
    // 没有文件格式, 无法得知元信息, 则默认全部为 use
    memset(m_pages, 0, MAXPAGES * sizeof(int));
    for (int i = 0; i < m_num_pages; i++) {
        set_use(i, 1);
    }
    return m_curr_file != nullptr;
}

int DataStorageManager::close_file()
{
    fclose(m_curr_file);
    m_curr_file = nullptr;
    m_num_pages = 0;
    return 0;
}

int DataStorageManager::read_page(int page_id, char *frame)
{
    fseek(m_curr_file, page_id * PAGESIZE, SEEK_SET);
    io_count++;
    return fread(frame, PAGESIZE, 1, m_curr_file);
}

int DataStorageManager::write_page(int page_id, const char *frame)
{
    fseek(m_curr_file, page_id * PAGESIZE, SEEK_SET);
    io_count++;
    return fwrite(frame, PAGESIZE, 1, m_curr_file);
}

int DataStorageManager::seek(int offset, int pos)
{
    return fseek(m_curr_file, offset, pos);
}

FILE* DataStorageManager::get_file()
{
    return m_curr_file;
}

void DataStorageManager::inc_num_pages()
{
    m_num_pages++;
    fseek(m_curr_file, 0, SEEK_END);
    fwrite(page_default_content, PAGESIZE, 1, m_curr_file);
    set_use(m_num_pages - 1, 1);
    io_count++;
}

int DataStorageManager::get_num_pages()
{
    return m_num_pages;
}

void DataStorageManager::set_use(int index, int use_bit)
{
    m_pages[index] = use_bit;
}

int DataStorageManager::get_use(int index)
{
    return m_pages[index];
}

