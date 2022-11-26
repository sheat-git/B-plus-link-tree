#include <atomic>
#include "key.hpp"
#include "value.hpp"

class Tree;
class Node;

class Tree {
private:
    Node *root;

public:
    Tree();
    void traverse(bool showKeys);
    void insert(Key key, Value *value);
    Value *search(Key key);
    bool check();
};

class Node {
private:
    std::atomic_int info;
    std::atomic_int size;
    Key *keys;
    Key highKey;
    Node *next;
    const bool isLeaf;
    union {
        Node **children;
        Value **values;
    };

private:
    // latchを1度試みる。成否が返り値
    bool attemptLatch(int oldInfo);
    void latch();
    void unlatch();
    void unlatch(int cache);
    // 引数の右側に分割されたものとして値をコピー
    void copyFromLeft(Node *left);
    void copyFromLeftInternal(Node *left);
    void copyFromLeftLeaf(Node *left);
    bool insertToInternal(Key key, Value *value);
    bool insertToLeaf(Key key, Value *value);
public:
    Node(bool isLeaf, int size);
    bool insert(Key key, Value *value);
    Value *search(Key key);
    void traverse(bool showKeys = true);
    bool check();

friend class Tree;
};
