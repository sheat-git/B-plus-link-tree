#include <thread>
#include <vector>
#include <random>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <stdlib.h>
#include <stdio.h>
#include "tree.hpp"

void insert(Tree *tree, std::vector<int>& keys) {
    for (unsigned i=0; i<keys.size(); i++) {
        tree->insert(keys[i], nullptr);
    }
}

long long bench(int threadCount, int dataCount, bool traverse) {

    // keysは等差数列をシャッフルしたもの
    // keysのサイズ確保
    std::vector<std::vector<int> > keys(threadCount);
    for (int i=0; i<dataCount%threadCount; i++) {
        keys[i] = std::vector<int>(dataCount/threadCount+1);
    }
    for (int i=dataCount%threadCount; i<threadCount; i++) {
        keys[i] = std::vector<int>(dataCount/threadCount);
    }
    // keysに代入
    for (int i=0; i<dataCount; i++) {
        keys[i%threadCount][i/threadCount] = i;
    }
    // keysの中身をシャッフル
    std::mt19937_64 get_rand_mt(0);
    for (int i=0; i<threadCount; i++) {
        std::shuffle( keys[i].begin(), keys[i].end(), get_rand_mt );
    }

    // tree作成
    Tree *tree = new Tree();

    // threadsのサイズ確保
    std::vector<std::thread> threads(threadCount);

    std::chrono::system_clock::time_point  start, end;

    start = std::chrono::system_clock::now();

    // スレッドで実行
    for (int i=0; i<threadCount; i++) {
        threads[i] = std::thread(insert, tree, std::ref(keys[i]));
    }

    // 待ち
    for (auto& t: threads) {
        t.join();
    }

    end = std::chrono::system_clock::now();

    if (traverse) tree->traverse(true);

    if (!tree->check(1)) return -1;

    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

int main(int argc, char *argv[]) {
    long long times = 0, time;
    int threadCount, dataCount;

    if (argc != 3) {
        std::cerr << "give threadCount and dataCount\n";
        return 1;
    }

    threadCount = atoi(argv[1]);
    dataCount = atoi(argv[2]);

    if (threadCount > (int) std::thread::hardware_concurrency()) {
        std::cerr << "too many threads (limit:"
            << std::thread::hardware_concurrency() << ")\n";
        return 1;
    }

    for (int i=0; i<100; i++) {
        std::cout << i << "... ";
        time = bench(threadCount, dataCount, false);
        if (time == -1) {
            std::cerr << "insert fault\n";
            return 1;
        }
        std::cout << "success\n";
        times += time;
    }
    std::cout << "Avg. " << (double) times / 100 << "msec\n";
    return 0;
}
