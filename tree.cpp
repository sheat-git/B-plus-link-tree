#include <iostream>
#include "tree.hpp"

#define MIN_DEG 3
#define MAX_DEG 2*MIN_DEG-1

int turnOnLSB(int info) {
    return info | 1;
}

int turnOffLSB(int info) {
    return info & ~1;
}

Tree::Tree() {
    root = new Node(true, 0);
}

void Tree::traverse(bool showKeys) {
    root->traverse(showKeys);
}

void Tree::insert(Key key, Value *value) {

    Node *rootCache;
    int rootInfoCache;

    // 木を深くするとき用
    Node *newRoot = nullptr, *right;

    while (true) {

        // cacheの初期化（更新）
        rootCache = root;
        rootInfoCache = turnOffLSB(rootCache->info);

        // root->sizeがいっぱいなら次で木を深くする
        if (rootCache->size != MAX_DEG) {

            // rootが更新されていたらcacheも更新して再実行
            if (root != rootCache) {
                rootCache = root;
                rootInfoCache = turnOffLSB(rootCache->info);
                continue;
            }
            if (rootCache->info != rootInfoCache) {
                rootInfoCache = turnOffLSB(rootCache->info);
                continue;
            }

            // insertに成功したら終了
            if (root->insert(key, value, nullptr)) return;
            // 失敗したら再実行
            else continue;
        }

        // ループの最初のみ
        if (newRoot == nullptr) {

            // newRootとrightを作成
            newRoot = new Node(false, 1);
            if (rootCache->isLeaf) {
                newRoot->keys[0] = rootCache->keys[MIN_DEG];
                right = new Node(true, MIN_DEG-1);
                right->copyFromLeftLeaf(rootCache);
            } else {
                newRoot->keys[0] = rootCache->keys[MIN_DEG-1];
                right = new Node(false, MIN_DEG-1);
                right->copyFromLeftInternal(rootCache);
            }
            newRoot->children[0] = rootCache;
            newRoot->children[1] = right;

        }
        // 2回目以降
        else {

            // newRootを更新
            newRoot->children[0] = rootCache;

            // rightを更新
            if (rootCache->isLeaf) {
                if (!right->isLeaf) {
                    right = new Node(true, MIN_DEG-1);
                    newRoot->children[1] = right;
                }
                right->copyFromLeftLeaf(rootCache);
            } else {
                if (right->isLeaf) {
                    right = new Node(false, MIN_DEG-1);
                    newRoot->children[1] = right;
                }
                right->copyFromLeftInternal(rootCache);
            }

        }

        // すでに試みられていたら再実行
        if (!rootCache->attemptLatch(rootInfoCache)) continue;

        // rootがすでに新しいNodeで更新されていたら次へ
        if (root != rootCache) {
            rootCache->unlatch();
            continue;
        }

        // rootの値を更新する
        if (rootCache->isLeaf) {
            rootCache->size = MIN_DEG;
        } else {
            rootCache->size = MIN_DEG-1;
        }
        rootCache->highKey = newRoot->keys[0];
        rootCache->next = right;
        root = newRoot;
        rootCache->unlatch(rootInfoCache);

        // insertに成功したら終了
        // 失敗したら再実行
        if (root->insert(key, value, nullptr)) return;

    }
}

Value *Tree::search(Key key) {
    return root->search(key);
}

bool Tree::check() {
    return root->check();
}

Node::Node(bool isLeaf, int size): isLeaf(isLeaf) {
    info = 0;
    this->size = size;
    keys = new Key[MAX_DEG];
    next = nullptr;
    if (isLeaf) {
        values = new Value *[MAX_DEG];
    } else {
        children = new Node *[MAX_DEG+1];
    }
}

bool Node::attemptLatch(int expected) {
    return info.compare_exchange_weak(expected, turnOnLSB(expected));
}

void Node::latch() {
    int expected;
    do {
        expected = turnOffLSB(info);
    } while (!attemptLatch(expected));
}

void Node::unlatch() {
    info++;
}

void Node::unlatch(int cache) {
    info = cache + 0b10;
}

void Node::copyFromLeft(Node *left) {
    if (isLeaf) copyFromLeftLeaf(left);
    else copyFromLeftInternal(left);
}

void Node::copyFromLeftInternal(Node *left) {
    // keysとchildrenをコピー
    for (int i=0; i<MIN_DEG-1; i++) {
        keys[i] = left->keys[MIN_DEG+i];
        children[i] = left->children[MIN_DEG+i];
    }
    children[MIN_DEG-1] = left->children[MAX_DEG];
    // その他をコピー
    highKey = left->highKey;
    next = left->next;
}

void Node::copyFromLeftLeaf(Node *left) {
    // keysとvaluesをコピー
    for (int i=0; i<MIN_DEG-1; i++) {
        keys[i] = left->keys[MIN_DEG+i];
        values[i] = left->values[MIN_DEG+i];
    }
    // その他をコピー
    highKey = left->highKey;
    next = left->next;
}

bool Node::insertToInternal(Key key, Value *value, Node *parent) {

    int infoCache;
    int sizeCache, keyI;

    Node *childCache, *newChild = nullptr;
    int childInfoCache;

    while (true) {

        // infoCache初期化（更新）
        infoCache = turnOffLSB(info);

        // highKey以上はnextへ
        if (next != nullptr && highKey <= key) {
            if (info != infoCache) continue;
            return next->insertToInternal(key, value, parent);
        }

        // sizeCache初期化（更新）
        sizeCache = size;

        // すでに分割されているべきなのでparentへ
        if (sizeCache == MAX_DEG) {
            if (info != infoCache) continue;
            if (parent == nullptr) return false;
            return parent->insertToInternal(key, value, nullptr);
        }

        // keyを満たすインデックスkeyIを探す
        for (keyI=0; keyI<sizeCache; keyI++) {
            if (key < keys[keyI]) break;
        }

        // keyを満たすchild
        childCache = children[keyI];
        childInfoCache = turnOffLSB(childCache->info);

        // child->sizeがいっぱいなら次でchildを分割
        if (childCache->size != MAX_DEG) {
            if (info != infoCache || childCache->info != childInfoCache) continue;
            return childCache->insert(key, value, this);
        }

        // ループの最初のみ
        if (newChild == nullptr) {

            // newChildを作成
            if (childCache->isLeaf) {
                newChild = new Node(true, MIN_DEG-1);
                newChild->copyFromLeftLeaf(childCache);
            } else {
                newChild = new Node(false, MIN_DEG-1);
                newChild->copyFromLeftInternal(childCache);
            }

        }
        // 2回目以降
        else {

            // newChildを更新
            newChild->copyFromLeft(childCache);

        }

        // 自身とchildをlatch
        // 失敗したら再実行
        if (!attemptLatch(infoCache)) continue;
        if (!childCache->attemptLatch(childInfoCache)) {
            unlatch();
            continue;
        }

        // childの更新
        if (childCache->isLeaf) {
            childCache->size = MIN_DEG;
            childCache->highKey = childCache->keys[MIN_DEG];
        } else {
            childCache->size = MIN_DEG-1;
            childCache->highKey = childCache->keys[MIN_DEG-1];
        }
        childCache->next = newChild;
        childCache->unlatch(childInfoCache);

        // 自身の更新
        for (int i = sizeCache; i >= keyI+1; i--) {
            keys[i] = keys[i-1];
            children[i+1] = children[i];
        }
        keys[keyI] = childCache->highKey;
        children[keyI+1] = newChild;
        size = sizeCache+1;
        unlatch(infoCache);

        // 子へ
        return childCache->insert(key, value, this);

    }
}

bool Node::insertToLeaf(Key key, Value *value, Node *parent) {

    int infoCache, sizeCache;
    int keyI;

    while (true) {

        // infoCache初期化（更新）
        infoCache = turnOffLSB(info);

        // highKey以上はnextへ
        if (next != nullptr && highKey <= key) {
            if (info != infoCache) continue;
            return next->insertToLeaf(key, value, parent);
        }

        // sizeCache初期化（更新）
        sizeCache = size;

        // すでに分割されているべきなのでparentへ
        if (sizeCache == MAX_DEG) {
            if (info != infoCache) continue;
            if (parent == nullptr) return false;
            return parent->insertToInternal(key, value, nullptr);
        }

        // latchに失敗したら再実行
        if (!attemptLatch(infoCache)) continue;

        // 更新
        // keyより大きいものを1つ右へ
        for (keyI = sizeCache; keyI > 0; keyI--) {
            if (key >= keys[keyI-1]) break;
            keys[keyI] = keys[keyI-1];
            values[keyI] = values[keyI-1];
        }

        // 挿入
        keys[keyI] = key;
        values[keyI] = value;
        size = sizeCache+1;
        unlatch(infoCache);

        return true;

    }
}

bool Node::insert(Key key, Value *value, Node *parent) {
    if (isLeaf) return insertToLeaf(key, value, parent);
    else return insertToInternal(key, value, parent);
}

Value *Node::search(Key key) {

    // keyが満たすインデックスを探す
    int i = 0;
    int oldInfo = turnOffLSB(info);
    while (i < size && key < keys[i]) {
        i++;
    }

    // 最後まで来た時かつhighKey以上の時はnextのNodeでsearch
    // nextがnullptrのときは最も右のノードであり、highKeyが初期化されていないためチェック
    if (i >= size && next != nullptr && highKey <= key) {
        return next->search(key);
    }
    i--;

    if (isLeaf) {
        if (info == oldInfo) return values[i];
        return search(key);
    } else {
        if (info == oldInfo) return children[i]->search(key);
        return search(key);
    }
}

void Node::traverse(bool showKeys) {
    if (isLeaf) {
        for (int i=0; i<size; i++) {
            if (i==size-1) {
                showKeys ? std::cout << keys[i] << "\n" : std::cout << values[i] << "\n";
            } else {
                showKeys ? std::cout << keys[i] << " " : std::cout << values[i] << " ";
            }
        }
    } else {
        for (int i=0; i<=size; i++) {
            children[i]->traverse(showKeys);
        }
    }
}

bool Node::check() {
    if (!isLeaf) {
        // 再帰的に
        for (int i=0; i<size+1; i++) {
            if (!children[i]->check()) return false;
        }
        // nextが正しいか
        for (int i=0; i<size; i++) {
            if (children[i]->next != children[i+1]) return false;
        }
        // highKeyが正しいか
        for (int i=0; i<size; i++) {
            if (keys[i] != children[i]->highKey) return false;
        }
        if (next != nullptr && highKey != children[size]->highKey) return false;
    }
    // keysの並びは正しいか
    for (int i=0; i<size-1; i++) {
        if (keys[i] >= keys[i+1]) return false;
    }
    return true;
}
