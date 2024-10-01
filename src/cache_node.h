#ifndef FILE_CLASS_H
#define FILE_CLASS_H

#include <memory>

class Node {
   public:
    int sst_number;
    int size;
    Node* next;
    Node* prev;
    char bits;

    Node(int data, int size);
    void turnLastZeroToOne();
    void turnFirstOneToZero();
    void print();
    void setBit(int bit);
    void unsetBit(int bit);
    bool getBit(int bit);

    std::string string();
};

class FileClass {
   public:
    int sst_number;
    bool cached;
    int size;

    Node* filenode;

    FileClass(int sst_number);
    void markCached();

    void access();
};






#endif  // FILE_CLASS_H
