//
// Created by user on 02-06-2023.
//

#include "caching_system.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <sys/stat.h>

#include "cache_node.h"

#include "../../include/myposix.h"
#include "./tiering/thread_test.h"

#include <dirent.h>
#include "tiering/trivial.h"

#include "tiering/tiering.h"


#include "structs.h"
#include <libpmem.h>
#include <iostream>
#include <vector>
#include <set>
#include <regex>
#include <mutex>
#include <memory>

bool do_copy_to_pmem(const char* srcfile, const char* destfile) {

    // std::cout << "w0" << std::endl;

  char buf[4096];
  int cc;

    // std::cout << "w0.5" << std::endl;


  char *pmemaddr;
  size_t mapped_len;
  int is_pmem;

  //open source file read only
  int srcfd = open(srcfile, O_RDONLY);

  if (srcfd < 0) {
    std::cout << "copy to pmem: open failed" << std::endl;
    return false;
  } else {
    struct stat stat;
      // std::cout << "getting stat for srcfile " << srcfile << std::endl;
    if (fstat(srcfd, &stat) < 0) {
        close(srcfd);
        throw std::runtime_error(std::string("fstat failed"));
    }

    // std::cout << "w1" << std::endl;


    if ((pmemaddr = (char*)pmem_map_file(destfile, stat.st_size,
        PMEM_FILE_CREATE|PMEM_FILE_EXCL,
        0666, &mapped_len, &is_pmem)) == NULL) {
      perror("pmem_map_file");
      std::cout << "error: " << std::strerror(errno) << std::endl;
      //print disk space (/dev/pmem0)
      // std::cout << "disk space: " << std::endl;
      // system("df -h /dev/pmem0");
      std::cout << "pmem_error7: " << pmem_errormsg() << std::endl;
      // std::cout << "LEAVING: pmem_map_file returned null" << std::endl;
      // exit(1);
    } else {

      posix_fadvise(srcfd, 0, stat.st_size, POSIX_FADV_SEQUENTIAL);

      // std::cout << "size is " << stat.st_size << std::endl;
      ssize_t res = readahead(srcfd, 0, stat.st_size);
      // std::cout << "readahead result: " << res << std::endl;

      // std::cout << "w2" << std::endl;

      // bool on = true;
      // while (on) {

      //   for (int i = 0; i < 256; i++) {
      //     if ((cc = read(srcfd, buf, 4096)) > 0) {
      //       pmem_memcpy_nodrain(pmemaddr, buf, cc);
      //       // std::cout << "wrote to pmem" << std::endl;
      //       pmemaddr += cc;
      //     } else {
      //       on = false;
      //       break;
      //     }
      //   }

      //   std::this_thread::sleep_for(std::chrono::milliseconds(2));


      // }


      while ((cc = read(srcfd, buf, 4096)) > 0) {

        // std::cout << "writing to pmem" << std::endl;
      
        pmem_memcpy_nodrain(pmemaddr, buf, cc);

        // std::cout << "wrote to pmem" << std::endl;

        pmemaddr += cc;
      } 


      if (cc < 0) {
        perror("read");
        exit(1);
      }

      // std::cout << "w3" << std::endl;


      pmem_drain();

      posix_fadvise(srcfd, 0, stat.st_size, POSIX_FADV_DONTNEED);

      close(srcfd);
      pmem_unmap(pmemaddr, mapped_len);
    }
    
    return true;
  }

  
}

// add a new entry to non_cached_files_access_counter (unique lock)
void DeviceCache::add_non_cached_files_access_counter(int sst_number, Node_c* node) {
    std::unique_lock<std::shared_timed_mutex> lock(
        non_cached_files_access_counter_mutex);
    // std::cout << "about to insert node in non_cached_files_access_counter_c" << std::endl;
    non_cached_files_access_counter.insertNode_c(node);
}

// remove an entry from non_cached_files_access_counter (unique lock)
void DeviceCache::remove_non_cached_files_access_counter(Node_c* node) {
    std::unique_lock<std::shared_timed_mutex> lock(
        non_cached_files_access_counter_mutex);
    non_cached_files_access_counter.deleteNode_c(node);
}

// add a new entry to cached_files_access_counter (unique lock)
void DeviceCache::add_cached_files_access_counter(int sst_number, Node_c* node) {
    std::unique_lock<std::shared_timed_mutex> lock(
        cached_files_access_counter_mutex);
    // std::cout << "about to insert node in cached_files_access_counter_c" << std::endl;
    cached_files_access_counter.insertNode_c(node);
}

// remove an entry from cached_files_access_counter (unique lock)
void DeviceCache::remove_cached_files_access_counter(Node_c* node) {
    std::unique_lock<std::shared_timed_mutex> lock(
        cached_files_access_counter_mutex);
    current_cache_size -= 166289408;
    std::cout << "cache-" << cacheDevice->name << "- REMOVE - current size: " << current_cache_size << std::endl;
    cached_files_access_counter.deleteNode_c(node);
}

void DeviceCache::submit_to_cache_actual(Node_c* filenode) {
    FileAccessType access_type = SST_Read;
    bool is_pmem = true;

    //turn filenode->sst_number into a string
    std::string file_name = std::to_string(filenode->sst_number);

    // from the key create the file name (string with 6 digits - add the rest of
    // the zeros)
    while (file_name.length() < 6) {
        file_name = "0" + file_name;
    }
    // add the file extension
    file_name = file_name + getFileExtension();

    // std::unique_lock<std::shared_timed_mutex> lock1(
    //     non_cached_files_access_counter_mutex);
    // std::unique_lock<std::shared_timed_mutex> lock2(
    //     cached_files_access_counter_mutex);

    non_cached_files_access_counter.deleteNode_c(filenode);

    filenode->prev = NULL;
    filenode->next = NULL;
    
    cached_files_access_counter.insertNode_c(filenode);

    copy_info cp_info;
    cp_info.filename = file_name;
    cp_info.src_path = storageDevice->path + "/" + file_name;
    cp_info.dest_path = cacheDevice->path + "/" + file_name;
    cp_info.envFrom = storageDevice->env;
    cp_info.envTo = cacheDevice->env;

    add_file_to_cache_queue(cp_info);
}

void* DeviceCache::caching_thread() {
    // //print cache
    // print_cache_c();
    while (true) {
        {
            std::unique_lock<std::mutex> lock(hitratio_mutex);
            if (!check_hitratio_condition) {
                return NULL;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (cache_condition) {
            // std::this_thread::sleep_for(std::chrono::seconds(1));
            // sleep for 10 milliseconds
            // std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // transverse the non_cached_files_access_counter using the current_ptr
            std::unique_lock<std::shared_timed_mutex> lock1(
                non_cached_files_access_counter_mutex);
            std::unique_lock<std::shared_timed_mutex> lock2(
                cached_files_access_counter_mutex);

            std::pair<Node_c*,int> node_pair = non_cached_files_access_counter.select_most_used();
            Node_c* node = node_pair.first;

            if (node == NULL) {
                std::cout << "node selected2: NULL" << std::endl;
                continue;
            }
            //TODO - better threshold
            if (node_pair.second <= 4) {
                std::cout << "selected to cache was not accessed enough" << std::endl;
                continue;
            }

            // print
            std::cout << "node selected2: " << node->sst_number << " | hits: " << node_pair.second << std::endl;
            // submit to cache
            submit_to_cache_actual(node);

        }
    }
}

void* DeviceCache::check_hitratio() {
    // std::this_thread::sleep_for(std::chrono::seconds(10));

    while (true) {
        {
            std::unique_lock<std::mutex> lock(hitratio_mutex);
            if (!check_hitratio_condition) {
                return NULL;
            }
        }
        // std::this_thread::sleep_for(std::chrono::seconds(1));

        // if (storageDevice->name == 2) {
        //     std::cout << "global_posix_counter: " << global_posix_counter << std::endl;
        //     global_posix_counter = 0;
        // }

        storage_access_counter = 0;
        cache_access_counter = 0;
        // set_nvme_access_counter(0);
        // set_pmem_access_counter(0);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // float nvme = get_nvme_access_counter();
        // float pmem = get_pmem_access_counter();
        // std::cout << "nvme_counter: " << nvme << std::endl;
        // std::cout << "pmem_counter: " << pmem << std::endl;
        // std::cout << "hit ratio1: " << (float)pmem / (float)(nvme+pmem) << std::endl;


        int storage_access_counter_ = storage_access_counter;
        int cache_access_counter_ = cache_access_counter;
        float hit_ratio = 1;
        std::cout << "caching loop start" << std::endl;
        if (storage_access_counter_ + cache_access_counter_ != 0) {
            hit_ratio = (float)cache_access_counter_ / (float)(storage_access_counter_ + cache_access_counter_);

            std::cout << "storage_access_counter-" << storageDevice->name << ": " << storage_access_counter_ << std::endl;
            std::cout << "cache_access_counter-" << cacheDevice->name << ": " << cache_access_counter_ << std::endl;

            // std::cout << "nvme_counter2: " << nvme_counter << std::endl;
            // std::cout << "pmem_counter2: " << pmem_counter << std::endl;
        } else {
            std::cout <<  "storage_access_counter-" << storageDevice->name << " + " << "cache_access_counter-" << cacheDevice->name << " = 0" << std::endl;
            // std::cout << "nvme_counter + pmem_counter = 0" << std::endl;
            
        }
        std::stringstream msg;
        msg << "hit ratio-" << storageDevice->name << ": " << hit_ratio << std::endl;
        std::cout << msg.str();

        // if (hit_ratio < 0.7) {
        if (hit_ratio < 1) {
            cache_condition = true;
            std::cout << "cache condition: true" << std::endl;
        } 
        else {
            cache_condition = false;
            std::cout << "cache condition: false" << std::endl;

        }
    }
}


DeviceCache::DeviceCache(std::shared_ptr<Device> cacheDevice_, std::shared_ptr<Device> storageDevice_) : 
    cacheDevice(cacheDevice_), storageDevice(storageDevice_) {
    
    cache_queue = new std::queue<copy_info>;

    cache_condition = false;
    current_cache_size = 0;
    total_cache_size = cacheDevice->cache_size;

    check_hitratio_condition = true;

    // Declaration of thread condition variable
    new_file_cond1 = PTHREAD_COND_INITIALIZER;
    // declaring mutex
    queue_lock = PTHREAD_MUTEX_INITIALIZER;

    thread_hitratio = std::thread(&DeviceCache::check_hitratio, this);
    thread_caching = std::thread(&DeviceCache::caching_thread, this);
    thread_queue_worker = std::thread(&DeviceCache::queue_worker, this);

    // thread_hitratio.join();
    // thread_caching.join();
}

void DeviceCache::stopThreads() {
    {
        std::unique_lock<std::mutex> lock(hitratio_mutex);
        check_hitratio_condition = false;
        cache_condition = false;

        std::cout << "stopping threads" << std::endl;
    }
    thread_hitratio.join();
    thread_caching.join();
    {
        pthread_mutex_lock(&queue_lock);
        pthread_cond_signal(&new_file_cond1);
        pthread_mutex_unlock(&queue_lock);
    }
    thread_queue_worker.join();    
}

//function checks if file is in cache and unlinks the original file
void DeviceCache::unlink_checkIfCache(int sst_number) {

    Node_c* node = cached_files_access_counter.getNode(sst_number);
    if (node != NULL) {
        std::string file_name = std::to_string(sst_number);
        while (file_name.length() < 6) {
            file_name = "0" + file_name;
        }
        file_name = file_name + getFileExtension();
        std::string file_path = storageDevice->path + "/" + file_name;
        std::cout << "file " << sst_number << " is a cached file, calling unlink to: " << file_path << std::endl;
        unlink(file_path.c_str());
        remove_cached_files_access_counter(node);
    } else {
        std::cout << "file " << sst_number << " is not a cached file" << std::endl;
    }
}



//function checks if file is in either cached or non-cached list and removes it
void DeviceCache::removeSstTotal(int sst_number) {
    Node_c* node = non_cached_files_access_counter.getNode(sst_number);
    if (node != NULL) {
        remove_non_cached_files_access_counter(node);
    }
    Node_c* node2 = cached_files_access_counter.getNode(sst_number);
    if (node2 != NULL) {
        remove_cached_files_access_counter(node2);
    }
}


Node_c* DeviceCache::hasFile(int sst_number) {
    std::unique_lock<std::shared_timed_mutex> lock(
        cached_files_access_counter_mutex);
    Node_c* node = cached_files_access_counter.getNode(sst_number);
    return node;
}




void DeviceCache::add_file_to_cache_queue(copy_info cp_info) {
    pthread_mutex_lock(&queue_lock);
    cache_queue->push(cp_info);
    //   std::cout << "added file to trivial queue: " << file << std::endl;
    //   std::cout << "queue size: " << trivial_queue.size() << std::endl;
    pthread_cond_signal(&new_file_cond1);
    pthread_mutex_unlock(&queue_lock);
}

//Thread function
void* DeviceCache::queue_worker() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(hitratio_mutex);
            if (!check_hitratio_condition) {
                return NULL;
            }
        }

        // Lock mutex and then wait for signal to relase mutex
        pthread_mutex_lock(&queue_lock);
        while (cache_queue->empty() && check_hitratio_condition) {
            pthread_cond_wait(&new_file_cond1, &queue_lock);
        }

        if (!check_hitratio_condition) {
            pthread_mutex_unlock(&queue_lock);
            return NULL;
        }

        // Get the first element from queue
        copy_info cp_info = cache_queue->front();
        cache_queue->pop();

        // Unlock mutex
        pthread_mutex_unlock(&queue_lock);
        // print the first element from the file pair

        // hardcoded for now TODO fix
        struct stat st;
        stat(cp_info.src_path.c_str(), &st);

        int sst_number = std::stoi(cp_info.filename.substr(0, cp_info.filename.size() - 4));
        long file_size = st.st_size;

        if (current_cache_size + file_size < total_cache_size) {


            current_cache_size += file_size;
            std::cout << "cache-" << cacheDevice->name << "- ADD - current size: " << current_cache_size << std::endl;
            bool copied = inter_device_copy(cp_info);

                if (copied) {

                    std::cout << "succefully copied file: " << cp_info.src_path << " to " << cp_info.dest_path << std::endl;

                    FileAccessType access_type = SST_Read;
                    bool is_pmem = false;

                    if (cp_info.envTo == EnvEnum::PMDK) {
                        is_pmem = true;
                    }

                    reset_file_env((cp_info.dest_path),std::make_shared<Context>(access_type, is_pmem), cacheDevice, true);

                    std::cout << "cache copy finished! File: " << sst_number << " | level: " << get_sst_level(sst_number) << " *" << std::endl;
                
                    #ifdef PROFILER_3000
                    std::ostringstream oss;
                    oss << "h " << sst_number << " " << cacheDevice->name << std::endl;
                    writePROFILING(oss.str());
                    #endif  // PROFILER_3000
                
                
            }

        } else {
            std::cout << "Cache full! File: " << sst_number << " | level: " << get_sst_level(sst_number) << " *" << std::endl;
            continue;
        }
    }
}
