#include <iostream>
#include "tree.hpp"

#define MIN_DEG 3
#define MAX_DEG 2*MIN_DEG-1

unsigned turnOnLSB(unsigned info) {
    return info | 1;
}

unsigned turnOffLSB(unsigned info) {
    return info & ~1;
}

Tree::Tree() {
    root = new Node(true);
}

void Tree::traverse(bool showKeys) {
    root->traverse(showKeys);
}

void Tree::insert(Key key, Value *value) {
    Node *oldRoot;
    unsigned oldInfo;

    Node *newRoot, *right;

    std::stack<Node*> parents;

    while (true) {

        oldRoot = root;
        oldInfo = turnOffLSB(oldRoot->info);

        if (oldRoot->size == MAX_DEG) { // rootがいっぱいなので深くすることを試みる

            // newRootの準備をする
            newRoot = new Node(false);
            right = oldRoot->genSplittedRight();
            newRoot->size = 1;
            if (oldRoot->isLeaf) {
                newRoot->keys[0] = oldRoot->keys[MIN_DEG];
            } else {
                newRoot->keys[0] = oldRoot->keys[MIN_DEG-1];
            }
            newRoot->children[0] = oldRoot;
            newRoot->children[1] = right;

            // すでに試みられている場合、再実行
            if (!oldRoot->attemptLatch(oldInfo)) continue;
            // rootがすでに新しいNodeで更新されていた場合、再実行
            if (root != oldRoot) {
                oldRoot->unlatch();
                continue;
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
        }
        if (root->insert(key, value, parents)) break;
    }
}

Value *Tree::search(Key key) {
    return root->search(key);
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

bool Node::attemptLatch(unsigned expected) {
    return info.compare_exchange_strong(expected, turnOnLSB(expected));
}

void Node::latch() {
    unsigned expected;
    do {
        expected = turnOffLSB(info);
    } while (!attemptLatch(expected));
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

bool Node::insert(Key key, Value *value, std::stack<Node*>& parents) {

    unsigned oldInfo;

    int keyI;

    Node *node1, *node2;
    unsigned oldInfo1;

    while (true) {

        oldInfo = turnOffLSB(info);

        // highKey以上の時はnextへ
        if (next != nullptr && highKey <= key) {
            if (info != oldInfo) continue;
            return next->insert(key, value, parents);
        }

        // 親でsplitしているべきなので戻る
        if (size == MAX_DEG) {
            if (info != oldInfo) continue;
            if (parents.empty()) return false;
            node1 = parents.top();
            parents.pop();
            return node1->insert(key, value, parents);
        }

        if (isLeaf) {

            // 書き込み開始
            if (!attemptLatch(oldInfo)) continue;

            // keyより大きいものを1つ右へ
            keyI = size - 1;
            while (keyI >= 0 && key < keys[keyI]) {
                keys[keyI+1] = keys[keyI];
                values[keyI+1] = values[keyI];
                keyI--;
            }
            keyI++;

            // 挿入
            keys[keyI] = key;
            values[keyI] = value;
            size++;

            // 書き込み終了
            unlatch();
            return true;

        } else {

            // keyが満たすインデックスを探す
            keyI = size - 1;
            // size==MAX_DEG は再実行
            if (keyI == MAX_DEG-1) continue;
            while (keyI >= 0 && key < keys[keyI]) {
                keyI--;
            }
            keyI++;

            // keyが満たすchild
            node1 = children[keyI];
            oldInfo1 = turnOffLSB(node1->info);

            if (node1->size == MAX_DEG) {

                // newChildを準備
                node2 = node1->genSplittedRight();

                // 自身とchildをlatch。失敗したら再実行
                if (!attemptLatch(oldInfo)) continue;
                if (!node1->attemptLatch(oldInfo1)) {
                    unlatch();
                    continue;
                }

                // childの更新
                if (node1->isLeaf) {
                    node1->size = MIN_DEG;
                    node1->highKey = node1->keys[MIN_DEG];
                } else {
                    node1->size = MIN_DEG-1;
                    node1->highKey = node1->keys[MIN_DEG-1];
                }
                node1->next = node2;

                // childの書き込み終了
                node1->unlatch();

                // 自身の更新
                size++;
                for (int i = size-1; i >= keyI+1; i--) {
                    keys[i] = keys[i-1];
                    children[i+1] = children[i];
                }
                keys[keyI] = node1->highKey;
                children[keyI+1] = node2;

                // 書き込み終了
                unlatch();

                // 子へ
                parents.push(this);
                return node1->insert(key, value, parents);

            } else if (info == oldInfo && node1->info == oldInfo1) {

                // 子へ
                parents.push(this);
                return node1->insert(key, value, parents);

            }
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
