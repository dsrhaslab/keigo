//
// Created by user on 02-06-2023.
//

#ifndef TESTING_CACHING_SYSTEM_H
#define TESTING_CACHING_SYSTEM_H

#include "cache_node.h"

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <queue>
#include <vector>
#include <iostream>
#include <atomic>

#include <chrono>
#include <thread>
#include <shared_mutex>

#include <sstream>
#include <algorithm>

#include "structs.h"
#include "device.h"

class DeviceCache {
public:
    DeviceCache(std::shared_ptr<Device> cacheDevice_, std::shared_ptr<Device> storageDevice_);

    std::shared_ptr<Device> cacheDevice;
    std::shared_ptr<Device> storageDevice;
    long total_cache_size;
    long current_cache_size;


    int storage_access_counter = 0;
    int cache_access_counter = 0;

    std::thread thread_hitratio;
    std::mutex hitratio_mutex;
    std::thread thread_caching;
    std::mutex caching_mutex;
    std::thread thread_queue_worker;
    std::mutex queue_worker_mutex;

    // Declaration of thread condition variable
    pthread_cond_t new_file_cond1;
    // declaring mutex
    pthread_mutex_t queue_lock;

    void stopThreads();

    Linkedlist_c non_cached_files_access_counter;
    std::shared_timed_mutex non_cached_files_access_counter_mutex;

    Linkedlist_c cached_files_access_counter;
    std::shared_timed_mutex cached_files_access_counter_mutex;

    std::atomic<bool> cache_condition;
    std::atomic<bool> check_hitratio_condition;


    std::queue<copy_info>* cache_queue;
    
    Node_c* hasFile(int sst_number);

    void removeSstTotal(int sst_number);

    void add_non_cached_files_access_counter(int sst_number, Node_c* node);
    void remove_non_cached_files_access_counter(Node_c* node);
    void add_cached_files_access_counter(int sst_number, Node_c* node);
    void remove_cached_files_access_counter(Node_c* node);
    void submit_to_cache_actual(Node_c* filenode);
    void* caching_thread();
    void* check_hitratio();
    void add_file_to_cache_queue(copy_info cp_info);
    void* queue_worker();

    void unlink_checkIfCache(int sst_number);
};


#endif //TESTING_CACHING_SYSTEM_H
