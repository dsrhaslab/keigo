#include "caching_system.h"

#include <iostream>

#include "cache_node.h"


void Node::turnLastZeroToOne() {
    // if bits are all 1 then return
    if (bits == 255) {
        return;
    }

    char mask = 1;

    while ((this->bits & mask) != 0) {
        mask <<= 1;
    }
    this->bits |= mask;
}

void Node::turnFirstOneToZero() {
    if (bits == 0) {
        return;
    }

    char mask = 1 << 7;  // Start with a mask having the leftmost bit set to 1

    while (mask != 0) {
        if ((this->bits & mask) != 0) {
            this->bits &= ~mask;  // Set the first encountered 1 bit to 0
            break;
        }
        mask >>= 1;  // Shift the mask to the right to check the next bit
    }
}

void Node::print() {
    for (int i = 7; i >= 0; --i) {
        bool bit = (bits >> i) & 1;
        std::cout << bit;
    }
    std::cout << std::endl;
}

std::string Node::string() {
    std::string s;
    for (int i = 7; i >= 0; --i) {
        bool bit = (bits >> i) & 1;
        s += std::to_string(bit);
    }
    return s;
}

void Node::setBit(int bit) { bits |= (1 << bit); }

void Node::unsetBit(int bit) { bits &= ~(1 << bit); }

bool Node::getBit(int bit) { return bits & (1 << bit); }



















Node_c::Node_c(int sst_number_, int size_) {
    sst_number = sst_number_;
    size = size;
    next = nullptr;
    prev = nullptr;
    counter = 0;
}

void Node_c::addOne() {
    counter += 1;
}

int Node_c::getCounter() {
    return counter;
}

void Node_c::decay(int decay_rate) {
    counter -= decay_rate;
    if (counter < 0) {
        counter = 0;
    }
}

std::string Node_c::string() {
    std::string s;
    s += std::to_string(counter);
    return s;
}