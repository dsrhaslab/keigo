#include "structs.h"
#include <shared_mutex>
#include <mutex>
#include <iostream>


// Function to delete the
// node at given position
void Linkedlist_c::deleteNode_c(Node_c* todelete) {

    // If list is empty
    if (head == NULL) return;

    this->node_length--;
    // check if it is the header, since if it is being deleted, the header will
    // change. also check if it is the current_ptr, since if it is being
    // deleted, the current_ptr will change. Since the linkedlist is circular
    // and doubly linked, we can just change the pointers of the previous and
    // next nodes.

    bool isHeader = false;
    bool isCurrent = false;
    bool isLast = false;

    if (todelete == head) {
        isHeader = true;
    }
    if (todelete == current_ptr) {
        isCurrent = true;
    }
    if (todelete == last_node) {
        isLast = true;
    }

    if (todelete->prev == NULL || todelete->next == NULL) {
        return;
    }

    todelete->prev->next = todelete->next;
    todelete->next->prev = todelete->prev;


    // If header is being deleted, update the header
    if (isHeader) {
        head = todelete->prev;
    }

    // If current_ptr is being deleted, update the current_ptr
    if (isCurrent) {
        current_ptr = todelete->prev;
    }

    // If last_node is being deleted, update the last_node
    if (isLast) {
        last_node = todelete->prev;
    }

}

// Function to insert a new node.
void Linkedlist_c::insertNode_c(int data, int size) {
    this->node_length++;
    // Create the new Node.
    Node_c* newNode = new Node_c(data, size);

    // Assign to head
    if (head == NULL) {
        head = newNode;
        last_node = head;
        current_ptr = head;
        return;
    }

    // add it next to the last_node, update the last_node and make the header
    // the next to the last_node (circular linked list)
    last_node->next = newNode;
    newNode->prev = last_node;
    newNode->next = head;
    last_node = newNode;

    head->prev = last_node;
}

// Function to insert a new node.
void Linkedlist_c::insertNode_c(Node_c* newNode) {
    // std::cout << "inserting node " << "sst number: " << newNode->sst_number << std::endl;
    this->node_length++;
    // Assign to head
    if (head == NULL) {
        head = newNode;
        last_node = head;
        current_ptr = newNode;
        return;
    }

    // add it next to the last_node, update the last_node and make the header
    // the next to the last_node (circular linked list)
    last_node->next = newNode;
    newNode->prev = last_node;
    newNode->next = head;
    last_node = newNode;

    head->prev = last_node;
}

// Function to print the
// nodes of the linked list.
void Linkedlist_c::printList_c() {
    Node_c* temp = head;

    // Check for empty list.
    if (head == NULL) {
        std::cout << "List empty" << std::endl;
        return;
    }

    // print the linked list (given the length since it is circular)
    for (int i = 0; i < this->node_length; i++) {
        std::cout << temp->sst_number << " " << temp->string() << std::endl;
        temp = temp->next;
    }
}

 
//node and number of times accessed (counter)
std::pair<Node_c*,int> Linkedlist_c::select_most_used() {

    // std::unique_lock<std::shared_timed_mutex> lock1(
    //     non_cached_files_access_counter_mutex_c);
    // std::unique_lock<std::shared_timed_mutex> lock2(
    //     cached_files_access_counter_mutex_c);


    // std::cout << this->node_length << std::endl;

    //if the list is empty, return NULL
    if (this->node_length == 0) {
        return std::make_pair<Node_c*, int>(NULL, 0);
    }

    int highest = 0;
    Node_c* highest_node = NULL;

    current_ptr = head;

    //iterate through the list and find the node with the highest counter until we reach the head again
    for (int i = 0; i < this->node_length; i++) {
        if (current_ptr->getCounter() > highest) {
            highest = current_ptr->getCounter();
            highest_node = current_ptr;
            current_ptr->decay(10);
        }
        current_ptr = current_ptr->next;
    }

    return  std::make_pair(highest_node, highest);
}


Node_c* Linkedlist_c::getNode(int sst_number) {

    if (this->node_length == 0) {
        return NULL;
    }

    current_ptr = head;

    //iterate through the list and find the node with the highest counter until we reach the head again
    for (int i = 0; i < this->node_length; i++) {
        if (current_ptr->sst_number == sst_number) {
            return current_ptr;
        }
        current_ptr = current_ptr->next;
    }
    if (current_ptr && current_ptr->sst_number == sst_number) {
        return current_ptr;
    } else {
        return NULL;
    }
}