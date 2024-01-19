# Storage and Buffer Manager

2023《高级数据库系统》课程实验, 实现一个简单的 Storage and Buffer Manager. 实验分别为采用了 LRU, MRU, RANDOM, CLOCK, LRU-2 和 2Q 缓冲区替换算法, 并统计了访问总次数、命中次数、命中率、磁盘 I/O 次数以及执行时间.

## 运行

编译
```sh
cmake -S . -B build
cd build
cmake ..
cd ..
cmake --build build
```
运行不同的替换算法
```sh
./build/adblab lru
./build/adblab mru
./build/adblab random
./build/adblab clock
./build/adblab lru-2
./build/adblab 2q
```
