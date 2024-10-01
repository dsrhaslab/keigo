#include "global.h"

#include <map>
#include <mutex>
#include <set>
#include <string>

#include <iostream>
#include <thread>
#include <shared_mutex>
#include <list>

#include <atomic>
#include <memory>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <cstring>


// #include "../../include/myposix.h"
#include "./tiering/thread_test.h"

std::atomic<int> writer_threads_num(0);


//set of trivially moved sst numbers (string)
std::set<std::string> trivially_moved_sst_numbers;
std::mutex trivially_moved_sst_numbers_lock;

//add a file to the set
void add_trivial_sst_file(std::string file_number) {
    trivially_moved_sst_numbers_lock.lock();
    trivially_moved_sst_numbers.insert(file_number);
    trivially_moved_sst_numbers_lock.unlock();
}

//remove a file from the set
void remove_trivial_sst_file(std::string file_number) {
    trivially_moved_sst_numbers_lock.lock();
    trivially_moved_sst_numbers.erase(file_number);
    trivially_moved_sst_numbers_lock.unlock();
}

//check if a file is in the set
bool is_trivial_sst_file(std::string file_number) {
    trivially_moved_sst_numbers_lock.lock();
    bool result = trivially_moved_sst_numbers.find(file_number) != trivially_moved_sst_numbers.end();
    trivially_moved_sst_numbers_lock.unlock();
    return result;
}




//map sst file number (string) to file descriptor (int)
std::map<std::string, int> sst_fd_map;
//mutex for sst_fd_map
std::mutex sst_fd_map_mutex;

//add a new sst file to sst_fd_map
int add_sst_file(std::string sst_file_name, int fd) {
    std::lock_guard<std::mutex> lock(sst_fd_map_mutex);
    sst_fd_map[sst_file_name] = fd;
    return 0;
}

//get the file descriptor of an existing sst file or 0 if it doesn't exist
int get_fd_from_sst_file(std::string sst_file_name) {
    std::lock_guard<std::mutex> lock(sst_fd_map_mutex);
    
    if (sst_fd_map.find(sst_file_name) == sst_fd_map.end()) {
        return 0;
    }
    return sst_fd_map[sst_file_name];

}

//remove an existing sst file from sst_fd_map
void remove_sst_file(std::string sst_file_name) {
    std::lock_guard<std::mutex> lock(sst_fd_map_mutex);
    sst_fd_map.erase(sst_file_name);
}


// int nvme_access_counter = 0;
// int pmem_access_counter = 0;

// void increment_nvme_access_counter() {
//     nvme_access_counter++;
// }

// void increment_pmem_access_counter() {
//     pmem_access_counter++;
// }

// //get the nvme access counter
// int get_nvme_access_counter() {
//     return nvme_access_counter;
// }

// //get the pmem access counter
// int get_pmem_access_counter() {
//     return pmem_access_counter;
// }

// //set the nvme access counter
// void set_nvme_access_counter(int value) {
//     nvme_access_counter = value;
// }

// //set the pmem access counter
// void set_pmem_access_counter(int value) {
//     pmem_access_counter = value;
// }

// void print_access_counters() {
//     std::cout << "nvme_access_counter: " << nvme_access_counter << std::endl;
//     std::cout << "pmem_access_counter: " << pmem_access_counter << std::endl;
// }





//map sst file number (int) to level (int)
std::map<int, int> sst_level_map;

//shared mutex for sst_level_map
std::shared_timed_mutex sst_level_map_mutex;

// add a new entry to sst_level_map
void add_sst_level(int sst_file_number, int level) {
    std::lock_guard<std::shared_timed_mutex> lock(sst_level_map_mutex);
    sst_level_map[sst_file_number] = level;
}

// get the level of an sst file
int get_sst_level(int sst_file_number) {
    std::shared_lock<std::shared_timed_mutex> lock(sst_level_map_mutex);
    return sst_level_map[sst_file_number];
}

//mutex for print_to_screen
std::mutex print_to_screen_mutex;

void print_to_screen(std::string message) {
    std::lock_guard<std::mutex> lock(print_to_screen_mutex);
    std::cout << message ;
}


//map sst file number to a list of strings
std::map<int, int> sst_file_number_to_string_list_map;
//shared mutex for sst_file_number_to_string_list_map
std::shared_timed_mutex sst_file_number_to_string_list_map_mutex;

//add a new entry to sst_file_number_to_string_list_map that doesnt exist (unique lock)
void add_sst_file_number_to_string_list_map(int sst_file_number) {
    std::unique_lock<std::shared_timed_mutex> lock(sst_file_number_to_string_list_map_mutex);
    sst_file_number_to_string_list_map[sst_file_number] = 0;
}

//add a string to the list of strings of an sst file number (shared lock)
void add_string_to_sst_file_number_to_string_list_map(int sst_file_number) {
    std::shared_lock<std::shared_timed_mutex> lock(sst_file_number_to_string_list_map_mutex);
    sst_file_number_to_string_list_map[sst_file_number]++;
}

//print the list of strings of all sst file number (shared lock)
void print_sst_file_number_to_string_list_map() {
    std::shared_lock<std::shared_timed_mutex> lock(sst_file_number_to_string_list_map_mutex);
    for (auto const& x : sst_file_number_to_string_list_map) {
        std::cout << x.first << ": " << x.second << std::endl;
    }
}



bool runNow = false;


void activate() {
    runNow = true;
}


bool getRunNow() {
    return runNow;
}

//map sst file number to size (size_t)  
std::map<int, size_t> sst_file_number_to_size_map;
//shared mutex for sst_file_number_to_size_map
std::shared_timed_mutex sst_file_number_to_size_map_mutex;

//add a new entry to sst_file_number_to_size_map that doesnt exist (unique lock)
void add_sst_file_number_to_size_map(int sst_file_number, size_t size) {
    std::unique_lock<std::shared_timed_mutex> lock(sst_file_number_to_size_map_mutex);
    sst_file_number_to_size_map[sst_file_number] = size;
}

//get the size of an sst file (shared lock)
size_t get_sst_file_size(int sst_file_number) {
    std::shared_lock<std::shared_timed_mutex> lock(sst_file_number_to_size_map_mutex);
    return sst_file_number_to_size_map[sst_file_number];
}

//remove an sst file from sst_file_number_to_size_map (unique lock)
void remove_sst_file_from_size_map(int sst_file_number) {
    std::unique_lock<std::shared_timed_mutex> lock(sst_file_number_to_size_map_mutex);
    sst_file_number_to_size_map.erase(sst_file_number);
}






long total_cache_size = 90l*1024l*1024l*1024l;
std::atomic<long> current_cache_size(0);

long getCurrentCacheSize_c() {
    return current_cache_size;
}

void addCurrentCacheSize_c(long size) {
    current_cache_size += size;
}

long getTotalCacheSize_c() {
    return total_cache_size;
}


//a set of ints, representing all the files currently in cache
std::set<int> files_in_cache;
//shared mutex for files_in_cache
std::shared_timed_mutex files_in_cache_mutex;

//add a file to files_in_cache (unique lock)
void add_file_to_files_in_cache(int sst_file_number) {
    std::unique_lock<std::shared_timed_mutex> lock(files_in_cache_mutex);
    files_in_cache.insert(sst_file_number);
}

//remove a file from files_in_cache (unique lock)
void remove_file_from_files_in_cache(int sst_file_number) {
    std::unique_lock<std::shared_timed_mutex> lock(files_in_cache_mutex);
    files_in_cache.erase(sst_file_number);
}

//check if a file is in files_in_cache (shared lock)
bool is_file_in_files_in_cache(int sst_file_number) {
    std::shared_lock<std::shared_timed_mutex> lock(files_in_cache_mutex);
    return files_in_cache.find(sst_file_number) != files_in_cache.end();
}





std::vector<std::shared_ptr<Device>> tiers;
std::vector<std::shared_ptr<DeviceCache>> caches;

std::map<int, std::shared_ptr<Device>> level_to_device_map;
std::map<int, std::shared_ptr<Device>> name_to_device_map;

std::vector<std::shared_ptr<Device>> capacity_devices;

std::string profiler_log_file = "./profiling.txt";
bool activate_cache = false;

bool trivial_move_active = true;

int profiling_fd;
int profiling_fd2;
std::mutex profiling_fd_mutex;
std::mutex profiling_fd_mutex2;


std::atomic<bool> separating;
std::atomic<bool> separating2;

std::thread thread_PROFILER;
std::thread thread_PROFILER2;




#ifdef PROFILER_3000

//global map of ssts hits
std::map<int, int> sst_hits;

std::shared_timed_mutex ssh_hits_mutex;

//method to add a hit to the sst_hits map
void add_sst_hit(int sst_number) {
    std::shared_lock<std::shared_timed_mutex> lock(ssh_hits_mutex);
    sst_hits[sst_number]++;
}

//method to get the number of hits of an sst
int get_sst_hits(int sst_number) {
    std::shared_lock<std::shared_timed_mutex> lock(ssh_hits_mutex);
    return sst_hits[sst_number];
}

//method to print the sst_hits map
void print_sst_hits() {
    std::shared_lock<std::shared_timed_mutex> lock(ssh_hits_mutex);
    for (auto const& x : sst_hits) {
        std::cout << x.first << ": " << x.second << std::endl;
    }
}

//method to remove the entry of an sst from the sst_hits map
void remove_sst_hit(int sst_number) {
    std::unique_lock<std::shared_timed_mutex> lock(ssh_hits_mutex);
    sst_hits.erase(sst_number);
}

//add an entry to the sst_hits map
void add_sst_hit_entry(int sst_number) {
    std::unique_lock<std::shared_timed_mutex> lock(ssh_hits_mutex);
    sst_hits[sst_number] = 0;
}

#endif  // PROFILER_3000



void threadPROFILERseparator() {
    while (separating) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        #ifdef PROFILER_3000
        {
            std::shared_lock<std::shared_timed_mutex> lock(ssh_hits_mutex);

            //go through the sst_hits map and print the entries
            for (auto const& x : sst_hits) {
                std::ostringstream oss;
                if (x.second > 0) {
                    oss << "# " << x.first << " " << x.second << std::endl;
                    writePROFILING(oss.str());
                }
            }
            //set all ocurrences of sst_hits to 0
            for (auto const& x : sst_hits) {
                sst_hits[x.first] = 0;
            }

            std::stringstream msg;
            msg << "threads: " << writer_threads_num << std::endl;
            writePROFILING(msg.str());

            // //iterate thorugh the capacity devices and print the current size
            // for (auto const& x : capacity_devices) {
            //     std::ostringstream oss;
            //     oss << "z " << x->name << " " << x->lru->current_size << std::endl;
            //     writePROFILING(oss.str());
            // }

            // //iterate through the device caches and print the current size
            // for (auto const& x : caches) {
            //     std::ostringstream oss;
            //     oss << "x " << x->storageDevice->name << " " << x->current_cache_size << std::endl;
            //     writePROFILING(oss.str());
            // }


        }
        #endif  // PROFILER_3000


        writePROFILING("---\n");
    }
}


void threadPROFILERseparator2() {
    while (separating2) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        #ifdef PROFILER_3000
        {
            //iterate thorugh the capacity devices and print the current size
            for (auto const& x : capacity_devices) {
                std::ostringstream oss;
                oss << "z " << x->name << " " << x->lru->current_size << std::endl;
                writePROFILING2(oss.str());
            }

            //iterate through the device caches and print the current size
            for (auto const& x : caches) {
                std::ostringstream oss;
                oss << "x " << x->storageDevice->name << " " << x->current_cache_size << std::endl;
                writePROFILING2(oss.str());
            }


        }
        #endif  // PROFILER_3000


        writePROFILING2("---\n");
    }
}


void stopPROFILING() {
    {
        std::lock_guard<std::mutex> lock(profiling_fd_mutex);
        std::cout << "setting separating to false" << std::endl;
        setSeparating(false);
    }
    std::cout << "joining thread_PROFILER" << std::endl;
    thread_PROFILER.join();
    close(profiling_fd);
}

void stopPROFILING2() {
    {
        std::lock_guard<std::mutex> lock(profiling_fd_mutex2);
        setSeparating2(false);
    }
    thread_PROFILER2.join();
    close(profiling_fd2);
}


void openPROFILINGfile() {

    setSeparating(true);

    std::lock_guard<std::mutex> lock(profiling_fd_mutex);
    profiling_fd = open(profiler_log_file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);

    if (profiling_fd == -1) {
        // If the file couldn't be opened, print an error message
        std::cout << "Error opening file " << ": " << strerror(errno) << std::endl;
        // strerror(errno) gives a string describing the error based on the value of errno
    }

    std::cout << "profiling_fd: " << profiling_fd << std::endl;

    thread_PROFILER = std::thread(threadPROFILERseparator);
}


void openPROFILINGfile2() {
    setSeparating2(true);
    std::lock_guard<std::mutex> lock(profiling_fd_mutex2);

    std::cout << "profiler opening file: " << (profiler_log_file+"2.txt") << std::endl;

    profiling_fd2 = open((profiler_log_file+"2.txt").c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);

    if (profiling_fd2 == -1) {
        // If the file couldn't be opened, print an error message
        std::cout << "Error opening file " << ": " << strerror(errno) << std::endl;
        // strerror(errno) gives a string describing the error based on the value of errno
    }

    std::cout << "profiling_fd2: " << profiling_fd2 << std::endl;

    thread_PROFILER2 = std::thread(threadPROFILERseparator2);
}


void writePROFILING(std::string line) {
    //lock the mutex
    std::lock_guard<std::mutex> lock(profiling_fd_mutex);
    write(profiling_fd, line.c_str(), line.size());
}

void writePROFILING2(std::string line) {
    //lock the mutex
    std::lock_guard<std::mutex> lock(profiling_fd_mutex2);
    write(profiling_fd2, line.c_str(), line.size());
}

int current_capacity_device_index = 0;

std::shared_ptr<Device> defaultDev = std::make_shared<Device>(-1,std::string("/home/gsd/ruben/kvs-nvme/kvstore"),std::string("posix"));

std::shared_ptr<Device> getDeviceFromLevel(int level) {
    //iterate through the map and return the device with the given level
    for (auto const& x : level_to_device_map) {
        if (x.first == level) {
            return x.second;
        }
    }
    //here, mapping doesn't exist

    

    if (capacity_devices[current_capacity_device_index]->lru->full) {
        std::cout << "capacity device " << capacity_devices[current_capacity_device_index]->name << " is full" << std::endl;
        current_capacity_device_index++;
    }
    level_to_device_map[level] = capacity_devices[current_capacity_device_index];

    capacity_devices[current_capacity_device_index]->lru->last_level = level;

    std::cout << capacity_devices[current_capacity_device_index]->lru->thisCapacity->name << "'s last level is " << level << std::endl;

    std::cout << "level " << level << " mapped to " << capacity_devices[current_capacity_device_index]->name << std::endl;


    // if (capacity_devices[current_capacity_device_index]->current_size + 162388 > capacity_devices[current_capacity_device_index]->total_size) {
    //     current_capacity_device_index++;
    // }

    // capacity_devices[current_capacity_device_index]->current_size += 162388;

    return capacity_devices[current_capacity_device_index];
    
}


void stopLRUs() {
    for (auto const& x : capacity_devices) {
        if (x->lru != nullptr) {
            x->lru->stop();
        }
    }
}

void setSeparating(bool value) {
    std::cout << "setting separating to " << value << std::endl;
    separating = value;
}

void setSeparating2(bool value) {
    std::cout << "setting separating2 to " << value << std::endl;
    separating2 = value;
}



std::atomic<int> global_posix_counter(0);


