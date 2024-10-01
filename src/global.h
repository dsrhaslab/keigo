#ifndef MY_GLOBAL_H
#define MY_GLOBAL_H

#include <map>
#include <mutex>
#include <string>
#include <list>
#include <shared_mutex>
#include <vector>
#include <queue>
#include <thread>
#include <atomic>
#include "../../include/myposix.h"

#include "device.h"
#include "caching_system.h"





void add_trivial_sst_file(std::string file_number);
void remove_trivial_sst_file(std::string file_number);
bool is_trivial_sst_file(std::string file_number);

int add_sst_file(std::string sst_file_name, int fd);
int get_fd_from_sst_file(std::string sst_file_name);
void remove_sst_file(std::string sst_file_name);


extern std::atomic<int> writer_threads_num;


void add_sst_level(int sst_file_number, int level);
int get_sst_level(int sst_file_number);

void print_to_screen(std::string message);



void add_sst_file_number_to_string_list_map(int sst_file_number);
void add_string_to_sst_file_number_to_string_list_map(int sst_file_number);
void print_sst_file_number_to_string_list_map();


void activate();
bool getRunNow();

void add_sst_file_number_to_size_map(int sst_file_number, size_t size);
size_t get_sst_file_size(int sst_file_number);
void remove_sst_file_from_size_map(int sst_file_number);


// void increment_nvme_access_counter();
// void increment_pmem_access_counter();
// void set_nvme_access_counter(int value);
// void set_pmem_access_counter(int value);
// int get_nvme_access_counter();
// int get_pmem_access_counter();



long getCurrentCacheSize_c();
void addCurrentCacheSize_c(long size);
long getTotalCacheSize_c();


//add a file to files_in_cache (unique lock)
void add_file_to_files_in_cache(int sst_file_number);
void remove_file_from_files_in_cache(int sst_file_number);
bool is_file_in_files_in_cache(int sst_file_number);


void reset_file_env(std::string filepath, std::shared_ptr<Context> ctx, std::shared_ptr<Device> device, bool is_caching);

extern std::vector<std::shared_ptr<Device>> tiers;
extern std::vector<std::shared_ptr<DeviceCache>> caches;

extern std::map<int, std::shared_ptr<Device>> level_to_device_map;
extern std::map<int, std::shared_ptr<Device>> name_to_device_map;

extern int current_capacity_device_index;


extern std::vector<std::shared_ptr<Device>> capacity_devices;

extern std::shared_ptr<Device> defaultDev;

extern std::string profiler_log_file;
extern bool activate_cache;


extern bool trivial_move_active;

extern std::atomic<int> global_posix_counter;



std::shared_ptr<Device> getDeviceFromLevel(int level);


void openPROFILINGfile();
void openPROFILINGfile2();

void writePROFILING(std::string line);
void writePROFILING2(std::string line);

void stopPROFILING();
void stopPROFILING2();

void setSeparating(bool value);
void setSeparating2(bool value);


void stopLRUs();



#ifdef PROFILER_3000

void add_sst_hit(int sst_number);
int get_sst_hits(int sst_number);
void print_sst_hits();
void remove_sst_hit(int sst_number);
void add_sst_hit_entry(int sst_number);

#endif  // PROFILER_3000



#endif  // MY_GLOBAL_H
