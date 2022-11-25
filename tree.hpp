#include <atomic>
#include <stack>
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
    std::atomic_uint info;
    int size;
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
    bool attemptLatch(unsigned oldInfo);
    void latch();
    void unlatch();
    // 引数の右側に分割されたものとして初期化
    void copyFromLeft(Node *left);
public:
    Node(bool isLeaf, int size = 0);
    bool insert(Key key, Value *value, std::stack<Node*>& parents);
    Value *search(Key key);
    void traverse(bool showKeys = true);
    bool check();

friend class Tree;
};
