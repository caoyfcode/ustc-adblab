#include <iostream>
#include <string>
#include <chrono>
#include <cstdio>
#include "data_storage.h"
#include "buffer.h"

#define NUM_PAGES 50000

int main(int argc, char **argv)
{
    bool parse_fail = false;
    std::string algo_name;
    Replacer::Algo algo;
    if (argc == 2) {
        algo_name = argv[1];
        if (algo_name == "lru") {
            algo = Replacer::LRU;
        } else if (algo_name == "mru") {
            algo = Replacer::MRU;
        } else if (algo_name == "random") {
            algo = Replacer::RANDOM;
        } else if (algo_name == "clock") {
            algo = Replacer::CLOCK;
        } else if (algo_name == "lru-2") {
            algo = Replacer::LRU_2;
        } else if (algo_name == "2q") {
            algo = Replacer::TWO_QUEUE;
        } else {
            parse_fail = true;
        }
    } else {
        parse_fail = true;
    }
    if (parse_fail) {
        std::cout << "error: wrong format, please use" << std::endl;
        std::cout << "    adblab [lru|mru|random|clock|lru-2|2q]" << std::endl;
        return -1;
    }
    std::string db_name = "data/data.dbf";
    std::string trace_file_name = "data/data-5w-50w-zipf.txt";
    FILE *trace_file = fopen(trace_file_name.c_str(), "r");
    auto *dsmgr = new DataStorageManager;
    dsmgr->open_file(db_name);
    while (dsmgr->get_num_pages() < NUM_PAGES) {
        // 没有使用 FixNewPage 进行构造, 因为按照 pdf 理解 FixNewPage 将影响 buffer_manager, 而此处目的仅仅为了获得一个初始的数据库
        dsmgr->set_use(dsmgr->get_num_pages(), 1);
        dsmgr->inc_num_pages();
    }
    dsmgr->io_count = 0;
    auto *bufmgr = new BufferManager {dsmgr, algo};
    int read_or_write, page_id;

    auto before = std::chrono::high_resolution_clock::now();
    while (fscanf(trace_file, "%d,%d", &read_or_write, &page_id) == 2) {
        bufmgr->fix_page(page_id, read_or_write);
        bufmgr->unfix_page(page_id);
        // std::cout << "access count: " << bufmgr->access_count << std::endl;
    }
    auto after = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(after - before).count();

    int io_count = dsmgr->io_count, access_count = bufmgr->access_count, hit_count = bufmgr->hit_count;
    double hit_rate = static_cast<double>(hit_count) / static_cast<double>(access_count);
    std::cout << algo_name + ": " << std::endl
        << "    access count: " << access_count << std::endl
        << "    hit count: " << hit_count << std::endl
        << "    hit rate: " << hit_rate << std::endl
        << "    io count: " << io_count << std::endl
        << "    time: " << duration << "s" << std::endl;
    delete bufmgr;
    dsmgr->close_file();
    delete dsmgr;
    return 0;
}