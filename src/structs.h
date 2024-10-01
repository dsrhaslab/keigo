#ifndef MY_STRUCTS
#define MY_STRUCTS

#include <string>
#include <vector>


// #include "../include/myposix.h"

struct Policy {
  std::string type;
  std::vector<int> levels;
  long threshold = 0;
};

enum EnvEnum {
    POSIX = 0,
    PMDK = 1,
    NOP = -1
};


struct copy_info {
    int sst_number;
    std::string filename;
    std::string src_path;
    std::string dest_path;
    EnvEnum envFrom;
    EnvEnum envTo;
    int dest_tier_name;
    int dest_level;
};



class Node_c {
   public:
    int sst_number;
    int size;
    Node_c* next;
    Node_c* prev;
    int counter;

    Node_c(int data, int size);
    void addOne();
    int getCounter();
    void decay(int decay_rate);
    std::string string();


};


// Linked list class to
// implement a linked list.
class Linkedlist_c {
    Node_c* head;

   public:
    int node_length;

    Node_c* current_ptr;
    Node_c* last_node;


    // Default constructor
    Linkedlist_c() { 
        head = NULL; 
        current_ptr = NULL;
        last_node = NULL;
        node_length = 0;
    }

    // Function to insert a
    // node at the end of the
    // linked list.
    void insertNode_c(int, int);
    void insertNode_c(Node_c*);

    // Function to print the
    // linked list.
    void printList_c();

    // Function to delete the
    // node at given position
    void deleteNode_c(Node_c*);

    std::pair<Node_c*,int> select_most_used();
    Node_c* getNode(int sst_number);
};



#endif  // MY_STRUCTS