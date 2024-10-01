//
// Created by user on 02-06-2023.
//

#ifndef MY_DEVICE_H
#define MY_DEVICE_H

#include <string>
#include "structs.h"
#include "lru.h"

class DeviceCache;

class Device {
   public:
    int name;
    long cache_size;
    long disk_size;
    std::shared_ptr<Device> next;
    std::shared_ptr<Device> prev;
    std::shared_ptr<DeviceCache> cache = nullptr;
    Policy policy;
    
    EnvEnum env;
    std::string path;

    std::shared_ptr<LRU> lru;
    bool lru_working = false;

    long current_size = 0;
    long total_size = 0;

    void init_lru();


    Device(int name_, std::string path_, std::string env_, long cache_size_, long disk_size_, Policy policy_);
    Device(int name_, std::string path_, std::string env_);


    void set_next(std::shared_ptr<Device> next_);

    void set_prev(std::shared_ptr<Device> prev_);

   private:
    EnvEnum get_env(std::string env);
};



#endif  // MY_DEVICE_H
