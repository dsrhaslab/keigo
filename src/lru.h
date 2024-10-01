#ifndef LRU_H
#define LRU_H

#include <condition_variable>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <utility>
#include <atomic>
#include <map>


// #include "device.h"

class Device;

using namespace std;


class SstAccesses {
    public:
        unordered_map<int, int> accesses;
        priority_queue<pair<int, int>, vector<pair<int, int>>,
                   greater<pair<int, int>>>
        leastAccessed;
};

class LRU {
   public:
    LRU(long total_size_, long threshold_);

    // unordered_map<int, int> accesses;
    // priority_queue<pair<int, int>, vector<pair<int, int>>,SstAccesses
    //                greater<pair<int, int>>>
    //     leastAccessed;

    //for each level store a structure that contains the accesses and the least accessed
    std::map<int, std::unique_ptr<SstAccesses>> level_to_sst_accesses;

    //mutex for the queue
    std::mutex lru_queue_mutex;

    long threshold;
    long total_size;  
    long current_size = 0;

    bool full;

    int last_level = 0;


    std::shared_ptr<Device> thisCapacity;
    std::shared_ptr<Device> nextCapacity;

    std::thread lru_thread;


    std::mutex lru_struct_mutex;
    std::condition_variable new_file_submitted;


    std::atomic<bool> justAdded;
    bool working;

    void sizeCheck();
    void* lruThreadWork();

    void access(int sst_number);

    void add(int sst_number);

    void remove(int sst_number);

    void changeLevel(int sst_number, int oldLevel, int newlevel);

    int leastAccessedInt();
    
    void updateLeastAccessed(int level);

    void stop();

    void print();

    std::string to_string();

};

#endif  // FILE_CLASS_H