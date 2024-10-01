
#include "../include/myposix.h"
#include "caching_system.h"
#include "structs.h"
#include "device.h"



Device::Device(int name_, std::string path_, std::string env_, long cache_size_, long disk_size_, Policy policy_) : 
    name(name_), path(path_), cache_size(cache_size_), disk_size(disk_size_), policy(policy_) {
        env = get_env(env_);
    }


Device::Device(int name_, std::string path_, std::string env_) : 
    name(name_), path(path_) {
        env = get_env(env_);
    }

void Device::set_next(std::shared_ptr<Device> next_) {
    next = next_;
}

void Device::set_prev(std::shared_ptr<Device> prev_) {
    prev = prev_;
}

EnvEnum Device::get_env(std::string env) {
    if (env == "posix") {
        return EnvEnum::POSIX;
    } else if (env == "pmdk") {
        return EnvEnum::PMDK;
    } else {
        return EnvEnum::POSIX;
    }
}
