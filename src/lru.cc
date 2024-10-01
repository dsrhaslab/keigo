#include "lru.h"
#include "structs.h"
#include "global.h"

#include <atomic>
#include <memory>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <cstring>
#include "tiering/thread_test.h"
#include "tiering/trivial.h"
#include "tiering/tiering.h"


#include <sys/stat.h>



LRU::LRU(long total_size_, long threshold_) {
    full = false;
    justAdded = false;
    working = true;

    total_size = total_size_;
    threshold = threshold_;

    //populate level_to_sst_accesses with 10 levels //TODO - this is maybe inefficient
    for (int i = 0; i < 10; i++) {
        level_to_sst_accesses[i] = std::make_unique<SstAccesses>();
    }

    lru_thread = std::thread(&LRU::lruThreadWork, this);

}

void* LRU::lruThreadWork() {
    // std::cout << "lru for " << thisCapacity.get()->name << " started" << std::endl;

    while (working) {
       
        std::unique_lock<std::mutex> lock(this->lru_struct_mutex);

        while (!justAdded && working) {
            // std::cout << "l1.2.1: " << pthread_self()  << std::endl;

            this->new_file_submitted.wait(lock);
        }
        lock.unlock();
        if (justAdded) {
            

            // std::cout << "just added signal received" << std::endl;
            while ((current_size) > (total_size - threshold)) {
            // std::cout << "current size: " << current_size << " total size: " << total_size << " threshold: " << threshold << std::endl;
            // while (false) {

                // lock.lock();
                // std::cout << "doing work" << std::endl;
                full = true;
                int sst_number = leastAccessedInt();
                // std::cout << "least accessed: " << sst_number << std::endl;

                if (sst_number != -1) {

                    std::string fileNumber = std::to_string(sst_number);
                    // it is 6 digits long
                    while (fileNumber.length() < 6) {
                    fileNumber = "0" + fileNumber;
                    }
                    // add the .sst extension
                    std::string filename = fileNumber + getFileExtension();


                    copy_info cp_info;
                    cp_info.filename = filename;
                    cp_info.src_path = thisCapacity.get()->path + "/" + filename;
                    cp_info.dest_path = nextCapacity.get()->path + "/" + filename;
                    cp_info.envFrom = thisCapacity.get()->env;
                    cp_info.envTo = nextCapacity.get()->env;
                    cp_info.dest_tier_name = nextCapacity.get()->name;
                    cp_info.dest_level = get_sst_level(sst_number);
                    //remove extension from filename
                    std::string filename_ = filename.substr(0, filename.find_last_of("."));
                    cp_info.sst_number = std::stoi(filename_);

                    remove(sst_number);

                    // lock.unlock();

                    // bool copied = false;

                    std::cout << "lru copying file: " << cp_info.src_path << " to " << cp_info.dest_path  << "level: "  << cp_info.dest_level << std::endl;

                    bool copied = inter_device_copy(cp_info);


                    if (copied) {
                        add_sst_level(cp_info.sst_number,cp_info.dest_level);
                        std::cout << "lru copied file: " << cp_info.src_path << " to " << cp_info.dest_path  << "level: "  << cp_info.dest_level << std::endl;

                        #ifdef PROFILER_3000
                        std::ostringstream oss;
                        oss << "m " << cp_info.sst_number << " " << cp_info.dest_tier_name << " l" << cp_info.dest_level << std::endl;
                        writePROFILING(oss.str());
                        #endif  // PROFILER_3000


                        FileAccessType access_type = SST_Read;
                        bool is_pmem = false; //TODO... DEPENDE

                        reset_file_env((cp_info.dest_path),std::make_shared<Context>(access_type, is_pmem), name_to_device_map[cp_info.dest_tier_name], false);

                        std::shared_ptr<Device> device = name_to_device_map[cp_info.dest_tier_name];

                        if (device->lru_working) {
                            // std::cout << "adding to lru after lru" << std::endl;

                    // struct stat stat_buf;
    
                    // if (stat(cp_info.dest_path.c_str(), &stat_buf) == 0) {
                    //     std::cout << "File3 size of " << cp_info.dest_path.c_str() << " is " << stat_buf.st_size << " bytes.\n";
                    // } else {
                    //     std::cerr << "Error: Could not get file statistics.\n";

                    // }

                            device->lru->add(cp_info.sst_number);
                        }

                        std::cout << "unlinking in lru: " << cp_info.src_path << std::endl;

                    // unlink(cp_info.src_path.c_str());

                        int u = unlink(cp_info.src_path.c_str());
                        //show error if unlink fails (perror)
                        if (u != 0)
                            std::cout << "Error unlinking file " << ": " << strerror(errno) << std::endl;

                    if (thisCapacity->cache) {
                        Node_c* node = thisCapacity->cache->hasFile(cp_info.sst_number);
                        if (node) {
                            std::cout << "file moved from lru was in cache: " << cp_info.sst_number << std::endl;
                            thisCapacity->cache->remove_cached_files_access_counter(node);
                            std::cout << "unlinking file in cache (moved by lru): " << (thisCapacity->cache->cacheDevice->path + "/" + filename) << std::endl;
                            int u = unlink((thisCapacity->cache->cacheDevice->path + "/" + filename).c_str());
                            //show error if unlink fails (perror)
                            if (u != 0)
                                std::cout << "Error unlinking file " << ": " << strerror(errno) << std::endl;

                            }
                        }

                    }

                } else {
                    // lock.unlock();
                }

                
                // std::cout << "finished work" << std::endl;

            }
            justAdded = false;        
        } 
        if(working == false) {
            return NULL;
        }

    }

}

void LRU::stop() {
    //lock mutex
    // pthread_mutex_lock(&queue_lock);
    // pthread_cond_signal(&new_file_cond);
    // pthread_mutex_unlock(&queue_lock);
    {

        std::lock_guard<std::mutex> guard(this->lru_struct_mutex);

        working = false;
        this->new_file_submitted.notify_all();
    }

    lru_thread.join();


}

void LRU::access(int sst_number) {

    int level = get_sst_level(sst_number);
    //get the accesses struct for the level
    SstAccesses* accesses_struct = level_to_sst_accesses[level].get();

    accesses_struct->accesses[sst_number]++;
    updateLeastAccessed(level);
}

void LRU::sizeCheck() {

}

void LRU::add(int sst_number) {


    std::unique_lock<std::mutex> lock(this->lru_struct_mutex);


    int level = get_sst_level(sst_number);

    std::cout << "adding file to lru: " << sst_number << " to " << thisCapacity.get()->name << ", level is " << level << std::endl;

    SstAccesses* accesses_struct = level_to_sst_accesses[level].get();

    //check if sst_number is already in the lru
    if (accesses_struct->accesses.find(sst_number) != accesses_struct->accesses.end()) {
        return;
    }


    current_size += 166289408;
    accesses_struct->accesses[sst_number] =
        0;  // Initialize access count to 0 for new sst_numbers
    updateLeastAccessed(level);

    // std::cout << "lru added file: " << sst_number << " to " << thisCapacity.get()->name << std::endl;
    // std::cout << "added-size is now " << thisCapacity.get()->name << " : " << current_size << std::endl;

    //print all sst_numbers in the lru (separator is a comma) (all levels)
    // for (int i = 0; i < 10; i++) {
    //     std::cout << "level " << i << ": ";
    //     for (auto it = level_to_sst_accesses[i]->accesses.begin(); it != level_to_sst_accesses[i]->accesses.end(); ++it) {
    //         std::cout << it->first << ", ";
    //     }
    //     std::cout << std::endl;
    // }


    justAdded = true;

    //print the file number that was added to the lru and the current size of the lru
    std::cout << "added file: " << sst_number << " to " << thisCapacity.get()->name << std::endl;
    std::cout << "size is now " << thisCapacity.get()->name << " : " << current_size << std::endl;

    // pthread_cond_signal(&new_file_cond);

    // pthread_mutex_unlock(&queue_lock);

    this->new_file_submitted.notify_all();





}

void LRU::remove(int sst_number) {
    std::unique_lock<std::mutex> lock(this->lru_struct_mutex);

    int level = get_sst_level(sst_number);
    SstAccesses* accesses_struct = level_to_sst_accesses[level].get();

    //if sst_number is not in the lru, return
    if (accesses_struct->accesses.find(sst_number) == accesses_struct->accesses.end()) {
        return;
    }

    std::cout << "removing file from lru: " << sst_number << " (level: " << level << ") " << " from " << thisCapacity.get()->name << " | last level: " << last_level << std::endl;
    current_size -= 166289408;

    std::cout << "removed-size is now " << thisCapacity.get()->name << " : " << current_size << std::endl;


    accesses_struct->accesses.erase(sst_number);
    updateLeastAccessed(level);

    // std::cout << "lru removed file: " << sst_number << " to " << thisCapacity.get()->name << std::endl;
    // std::cout << "-size is now " << thisCapacity.get()->name << " : " << current_size << std::endl;

}

void LRU::changeLevel(int sst_number, int oldLevel, int newlevel) {
    //remove the sst from the old access struct and add it to the new one
    std::unique_lock<std::mutex> lock(this->lru_struct_mutex);

    
    SstAccesses* old_accesses_struct = level_to_sst_accesses[oldLevel].get();
    SstAccesses* new_accesses_struct = level_to_sst_accesses[newlevel].get();

    if (old_accesses_struct->accesses.find(sst_number) != old_accesses_struct->accesses.end()) {
        old_accesses_struct->accesses.erase(sst_number);
    }
    
    if (new_accesses_struct->accesses.find(sst_number) == new_accesses_struct->accesses.end()) {
        new_accesses_struct->accesses[sst_number] = 0;
    }

    updateLeastAccessed(oldLevel);
    updateLeastAccessed(newlevel);


}


int LRU::leastAccessedInt() {
    std::lock_guard<std::mutex> lock(lru_queue_mutex);
    //lock queue_mutex

    SstAccesses* accesses_struct = level_to_sst_accesses[last_level].get();


    while (!accesses_struct->leastAccessed.empty() &&
           accesses_struct->accesses[accesses_struct->leastAccessed.top().second] != accesses_struct->leastAccessed.top().first) {
        accesses_struct->leastAccessed.pop();
    }

    if (!accesses_struct->leastAccessed.empty()) {
        return accesses_struct->leastAccessed.top().second;
    } else {
        return -1;  // Return -1 if there are no accesses yet
    }
}

void LRU::updateLeastAccessed(int level) {
    std::lock_guard<std::mutex> lock(lru_queue_mutex);

    SstAccesses* accesses_struct = level_to_sst_accesses[level].get();

    accesses_struct->leastAccessed = priority_queue<pair<int, int>, vector<pair<int, int>>,
                                   greater<pair<int, int>>>();
    for (auto it = accesses_struct->accesses.begin(); it != accesses_struct->accesses.end(); ++it) {
        accesses_struct->leastAccessed.push(make_pair(it->second, it->first));
    }
}

void LRU::print() {
    for (int i = 0; i < 10; i++) {
        std::cout << "level " << i << ": ";
        for (auto it = level_to_sst_accesses[i]->accesses.begin(); it != level_to_sst_accesses[i]->accesses.end(); ++it) {
            std::cout << it->first << ", ";
        }
        std::cout << std::endl;
    }
    //also print current size
    std::cout << "current size: " << current_size << std::endl;
}


std::string LRU::to_string() {
    std::string str = "";
    for (int i = 0; i < 10; i++) {
        str += "level " + std::to_string(i) + ": ";
        for (auto it = level_to_sst_accesses[i]->accesses.begin(); it != level_to_sst_accesses[i]->accesses.end(); ++it) {
            str += std::to_string(it->first) + ", ";
        }
        str += "\n";
    }
    //also print current size
    str += "current size: " + std::to_string(current_size);
    return str;
}