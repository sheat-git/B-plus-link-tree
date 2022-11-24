#include <thread>
#include <vector>
#include <random>
#include <algorithm>
#include <iostream>
#include "tree.hpp"

void insert(Tree *tree, std::vector<int>& keys) {
    for (int i=0; i<keys.size(); i++) {
        tree->insert(keys[i], nullptr);
    }
}

bool test(int threadCount, int dataCount, bool traverse) {

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
    std::mt19937_64 get_rand_mt;
    for (int i=0; i<threadCount; i++) {
        std::shuffle( keys[i].begin(), keys[i].end(), get_rand_mt );
    }

    // tree作成
    Tree *tree = new Tree();

    // threadsのサイズ確保
    std::vector<std::thread> threads(threadCount);

    // スレッドで実行
    for (int i=0; i<threadCount; i++) {
        threads[i] = std::thread(insert, tree, std::ref(keys[i]));
    }

    // 待ち
    for (auto& t: threads) {
        t.join();
    }

    if (traverse) tree->traverse(true);

    return tree->check();
}

int main() {
    std::cout << test(5, 100000, true) << "\n";
    return 0;
}
