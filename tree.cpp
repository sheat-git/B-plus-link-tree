#include <iostream>
#include "tree.hpp"

#define MIN_DEG 3
#define MAX_DEG 2*MIN_DEG-1

Tree::Tree() {
    root = new Node(true);
}

void Tree::traverse(bool showKeys) {
    root->traverse(showKeys);
}

void Tree::insert(Key key, Value *value) {
    Node *oldRoot = root;
    unsigned oldInfo = turnOffLSB(oldRoot->info);

    if (oldRoot->size == MAX_DEG) { // rootがいっぱいなので深くすることを試みる
        
        // newRootの準備をする
        Node *newRoot = new Node(false);
        Node *right = oldRoot->genSplittedRight();
        newRoot->size = 1;
        if (oldRoot->isLeaf) {
            newRoot->keys[0] = oldRoot->keys[MIN_DEG];
        } else {
            newRoot->keys[0] = oldRoot->keys[MIN_DEG-1];
        }
        newRoot->children[0] = oldRoot;
        newRoot->children[1] = right;

        // すでに試みられている場合、再実行
        if (!oldRoot->attemptLatch(oldInfo)) insert(key, value);
        // rootがすでに新しいNodeで更新されていた場合、再実行
        if (root != oldRoot) {
            oldRoot->unlatch();
            insert(key, value);
        }

        // 更新開始
        // oldRootの値を更新する
        if (oldRoot->isLeaf) {
            oldRoot->size = MIN_DEG;
        } else {
            oldRoot->size = MIN_DEG-1;
        }
        oldRoot->highKey = newRoot->keys[0];
        oldRoot->next = right;
        // root交代
        root = newRoot;
        oldRoot->unlatch();

    } else {
        std::stack<Node*> parents;
        if (!root->insert(key, value, parents)) insert(key, value);
    }
}

Value *Tree::search(Key key) {
    root->search(key);
}

bool Tree::check() {
    return root->check();
}

Node::Node(bool isLeaf): isLeaf(isLeaf) {
    info = 0;
    size = 0;
    keys = new Key[MAX_DEG];
    next = nullptr;
    if (isLeaf) {
        values = new Value *[MAX_DEG];
    } else {
        children = new Node *[MAX_DEG+1];
    }
}

unsigned turnOnLSB(unsigned info) {
    return info | 1;
}

unsigned turnOffLSB(unsigned info) {
    return info & ~1;
}

bool Node::attemptLatch(unsigned expected) {
    return !info.compare_exchange_strong(expected, turnOnLSB(expected));
}

void Node::latch() {
    unsigned expected;
    do {
        expected = turnOffLSB(info);
    } while (attemptLatch(expected));
}

void Node::unlatch() {
    info++;
}

Node *Node::genSplittedRight() {
    Node *right = new Node(isLeaf);
    right->size = MIN_DEG - 1;

    if (isLeaf) {
        // keysとvaluesをコピー
        for (int i=0; i<MIN_DEG-1; i++) {
            right->keys[i] = keys[MIN_DEG+i];
            right->values[i] = values[MIN_DEG+i];
        }
    } else {
        // keysとchildrenをコピー
        for (int i=0; i<MIN_DEG-1; i++) {
            right->keys[i] = keys[MIN_DEG+i];
            right->children[i] = children[MIN_DEG+i];
        }
        right->children[MIN_DEG-1] = children[MAX_DEG];
    }

    // その他のコピー
    right->highKey = highKey;
    right->next = next;

    return right;
}

bool Node::insert(Key key, Value *value, std::stack<Node*> parents) {

    unsigned oldInfo = turnOffLSB(info);

    // 親でsplitしているべきなので戻る
    if (size == MAX_DEG) {
        if (parents.empty()) return false;
        Node *parent = parents.top();
        parents.pop();
        return parent->insert(key, value, parents);
    }

    if (isLeaf) {

        // 書き込み開始
        if (!attemptLatch(oldInfo)) return insert(key, value, parents);

        // keyより大きいものを1つ右へ
        int i = size - 1;
        while (i >= 0 && key < keys[i]) {
            keys[i+1] = keys[i];
            values[i+1] = values[i];
            i--;
        }
        // 最後まで来た時かつhighKey以上の時はnextのNodeでinsert
        // nextがnullptrのときは最も右のノードであり、highKeyが初期化されていないためチェック
        if (i == size-1 && next != nullptr && highKey <= key) {
            unlatch();
            return next->insert(key, value, parents);
        }
        i++;

        // 挿入
        keys[i] = key;
        values[i] = value;
        size++;

        // 書き込み終了
        unlatch();
        return true;

    } else {

        // keyが満たすインデックスを探す
        int i = 0;
        while (i < size && key < keys[i]) {
            i++;
        }

        // 最後まで来た時かつhighKey以上の時はnextのNodeでinsert
        // nextがnullptrのときは最も右のノードであり、highKeyが初期化されていないためチェック
        if (i >= size && next != nullptr && highKey <= key) {
            return next->insert(key, value, parents);
        }
        i--;

        // keyが満たすchild
        Node *child = children[i];
        unsigned childOldInfo = turnOffLSB(child->info);

        if (child->size == MAX_DEG) {

            // newChildを準備
            Node *newChild = child->genSplittedRight();

            // 自身とchildをlatch。失敗したら再実行
            if (!child->attemptLatch(childOldInfo)) return insert(key, value, parents);
            if (!attemptLatch(oldInfo)) {
                child->unlatch();
                return insert(key, value, parents);
            }

            // childの更新
            if (child->isLeaf) {
                child->size = MIN_DEG;
                child->highKey = child->keys[MIN_DEG];
            } else {
                child->size = MIN_DEG-1;
                child->highKey = child->keys[MIN_DEG-1];
            }
            child->next = newChild;

            // childの書き込み終了
            child->unlatch();

            // 自身の更新
            for (int j = size; j >= i+1; j--) {
                keys[j] = keys[j-1];
                children[j+1] = children[j];
            }
            keys[i] = child->highKey;
            children[i+1] = newChild;
            size++;

            // 書き込み終了
            unlatch();

            // 子へ
            parents.push(this);
            return child->insert(key, value, parents);

        } else if (info == oldInfo) {

            // 子へ
            parents.push(this);
            return child->insert(key, value, parents);

        } else {

            // infoが更新されていたら再実行
            return insert(key, value, parents);

        }
    }
}

Value *Node::search(Key key) {

    // keyが満たすインデックスを探す
    int i = 0;
    unsigned oldInfo = turnOffLSB(info);
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
            if (next == nullptr && i==size-1) {
                showKeys ? std::cout << keys[i] : std::cout << values[i];
            } else {
                showKeys ? std::cout << " " << keys[i] : std::cout << " " << values[i];
            }
        }
    }
}

bool Node::check() {
    if (!isLeaf) {
        // 再帰的に
        for (int i=0; i<size+1; i++) {
            if (!children[i]) return false;
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
