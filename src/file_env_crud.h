#ifndef FILE_ENV_CRUD_H
#define FILE_ENV_CRUD_H

#include <libpmem.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <stack>
#include <vector>
#include <cstdarg>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>

#include "file_envs/file_env.h"
#include "file_envs/pmem_file_env.h"
#include "file_envs/posix_file_env.h"
#include "file_envs/sst_write_file_env.h"
#include "file_envs/wal_file_env.h"

#include <shared_mutex>



#define MAX_FILES 50000
// create a vector of std::unique_ptr<FileEnv> the size of MAX_FILES and initialize them to nullptr
std::vector<std::unique_ptr<FileEnv>> file_envs(MAX_FILES + 4);

std::vector<std::unique_ptr<FileEnv>> file_envs_backup(MAX_FILES + 4);

// create a stack called free_fds to keep track of free file descriptors
std::stack<uint32_t> free_fds;
// stack lock
std::mutex free_fds_mutex;




std::vector<std::unique_ptr<std::shared_timed_mutex>> files_locks(MAX_FILES + 4);

void init_file_locks() {
    for (int i = 0; i < MAX_FILES + 4; i++) {
        files_locks[i] = std::make_unique<std::shared_timed_mutex>();
    }
}







// initialize the stack with file descriptors with values 0 to MAX_FILES-1
void init_free_fds() {

    const size_t page_size = getpagesize();
    // the allocation size of the stt files
    const size_t sst_map_size = Roundup(64 << 20 /*64MB*/, page_size);

    assert((page_size & (page_size - 1)) == 0);

    // lock the stack
    for (uint32_t i = 4; i < MAX_FILES + 4; i++) {
        free_fds.push(i);
    }

    init_file_locks();
}

// get a free file descriptor from the stack
uint32_t get_free_fd() {
    std::unique_lock<std::mutex> lock(free_fds_mutex);
    uint32_t fd = free_fds.top();
    // std::cout << "get_free_fd: fd: " << fd << std::endl;
    //print size of stack
    // std::cout << "get_free_fd: free_fds.size(): " << free_fds.size() << std::endl;
    free_fds.pop();
    return fd;
}

void free_fd(uint32_t fd) {
    std::unique_lock<std::mutex> lock(free_fds_mutex);
    free_fds.push(fd);
    // std::cout << "freed_fd: " << fd << " | size: " << free_fds.size() << std::endl;
}

//TODO: refactor creates

// returns the fake_fd
uint32_t CreateFileEnvPosix(int fd, const char *pathname) {
    // std::cout << "CreateFileEnvPosix: fd: " << fd << " | pathname: " << pathname << std::endl;
    
    uint32_t fake_fd = get_free_fd();
    //write lock
    std::unique_lock<std::shared_timed_mutex> lock(*files_locks[fake_fd]);

    file_envs[fake_fd] = std::make_unique<PosixFileEnv>(fd, pathname, false);
    return fake_fd;
}

uint32_t CreateFileEnvPmem(int fd, const char *pathname) {
    // std::cout << "CreateFileEnvPmem: fd: " << fd << " | pathname: " << pathname << std::endl;
    uint32_t fake_fd = get_free_fd();
    //write lock
    std::unique_lock<std::shared_timed_mutex> lock(*files_locks[fake_fd]);
    file_envs[fake_fd] = std::make_unique<PmemFileEnv>(fd, pathname, false);
    return fake_fd;
}

uint32_t CreateWalReadFileEnv(int fd, const char *pathname) {
    uint32_t fake_fd = get_free_fd();
    //write lock
    std::unique_lock<std::shared_timed_mutex> lock(*files_locks[fake_fd]);

    // std::cout << "CreateWalReadFileEnv, real fd is " << fd << " | fake fd is " << fake_fd << std::endl;

    file_envs[fake_fd] = std::make_unique<ReadableWalFileEnv>(fd, pathname);
    return fake_fd;
}


uint32_t CreateSstWriteFileEnv(int fd, const char *pathname) {
    // std::cout << "CreateSstWriteFileEnv: fd: " << fd << " | pathname: " << pathname << std::endl;

    uint32_t fake_fd = get_free_fd();
    //write lock
    std::unique_lock<std::shared_timed_mutex> lock(*files_locks[fake_fd]);
    file_envs[fake_fd] = std::make_unique<SSTWriteFileEnv>(fd, pathname);
    return fake_fd;
}



uint32_t CreateWalWriteFileEnv(int fd, const char *pathname) {
    uint32_t fake_fd = get_free_fd();
    //write lock
    std::unique_lock<std::shared_timed_mutex> lock(*files_locks[fake_fd]);
    file_envs[fake_fd] = std::make_unique<WritableWalFileEnv>(fd, pathname);
    return fake_fd;
}

FileEnv *GetFileEnv(int fake_fd) {
    //read lock
    // std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fake_fd]);
    return file_envs[fake_fd].get();
}

void RemoveFileEnv(int fake_fd) {
    // std::cout << "RemoveFileEnv: fake_fd: " << fake_fd << std::endl;

    // write lock
    // std::unique_lock<std::shared_timed_mutex> lock(*files_locks[fake_fd]);
    free_fd(fake_fd);
    file_envs[fake_fd] = nullptr;
}












void ResetFileEnv(int fake_fd, int real_fd, std::string filepath, std::shared_ptr<Context> ctx, std::shared_ptr<Device> device, bool is_caching) {
    std::cout << "ResetFileEnv: fake_fd: " << fake_fd << " | real_fd: " << real_fd << " | filepath: " << filepath << " | device: " << device << std::endl;

    std::unique_lock<std::shared_timed_mutex> lock(*files_locks[fake_fd]);
    
    //get filename from filepath (fname_)
    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);
    //remove file extension
    filename = filename.substr(0, filename.find_last_of(".")); 
    // //get number from file name (remove .sst)
    int sst_number = std::stoi(filename);

    //get the file_env object
    // std::unique_ptr<FileEnv> file_env = std::move(file_envs[fake_fd]);

    // std::shared_ptr<FileEnv> file_env = file_envs[fake_fd];

    //could have been deleted in the meantime...
    if (fake_fd >= 0 && fake_fd < file_envs.size() && file_envs[fake_fd].get() != nullptr) {

        //call the destructor for file_envs[fake_fd] object
        // file_envs[fake_fd].reset();


        // std::cout << "linking file: " << sst_number << " to device: " << device << std::endl;
        // std::cout << "linking file: " << sst_number << " to device: " << device->name << std::endl;

        if (!is_caching) {
            file_envs[fake_fd].get()->force_destruct();

            // //call destructor for file_env object
            // file_envs[fake_fd].get()->~FileEnv();

            //call the destructor for file_envs[fake_fd] object
            file_envs[fake_fd].reset();
        }


        linkFileToDevice(sst_number, device);


        if (ctx->is_pmem_) {
            if (ctx->type_ == SST_Read) {
                //add previous to file_envs_backup
                file_envs_backup[fake_fd] = std::move(file_envs[fake_fd]);


                file_envs[fake_fd] = std::make_unique<PmemFileEnv>(real_fd, filepath, is_caching, true);
                // file_envs[fake_fd]->_open(filepath.c_str(), O_RDONLY, 0644);
                // std::unique_lock<std::shared_timed_mutex> lock(*files_locks[fake_fd]);


                // file_envs[fake_fd] = std::make_unique<PmemFileEnv>(real_fd, filepath, is_caching);
                // file_envs[fake_fd]->_open(filepath.c_str(), O_RDONLY, 0644);


            } else if (ctx->type_ == WAL_Read) {
                file_envs[fake_fd] = std::make_unique<ReadableWalFileEnv>(real_fd, filepath);
            } else {
                file_envs[fake_fd] = std::make_unique<PmemFileEnv>(real_fd, filepath, is_caching);
            }
            add_file_to_files_in_cache(sst_number);
        } else {
            file_envs[fake_fd] = std::make_unique<PosixFileEnv>(real_fd, filepath, is_caching);
            file_envs[fake_fd]->_open(filepath.c_str(), O_RDONLY, 0644);
        }




        // file_env->force_destruct();
        //call destructor for file_env object
        // delete file_env;
        
    }



    
}









// // initialize the stack with file descriptors with values 0 to MAX_FILES-1
// void init_free_fds() {
// }


// // map that for each file descriptor, it has a file object
// std::map<uint32_t, std::unique_ptr<FileEnv>> fd_map;
// //shared mutex for fd_map
// std::shared_timed_mutex fd_map_mutex;

// FileEnv *GetFileEnv(int fd) {
//     //return file_envs[fd].get(); or null if doesnt exist
//     std::shared_lock<std::shared_timed_mutex> lock(fd_map_mutex);
//     auto it = fd_map.find(fd);
//     if (it == fd_map.end()) {
//         return nullptr;
//     }
//     return it->second.get();

// }

// uint32_t CreateFileEnv(int fd, const char *pathname) {
//     // std::cout << "CreateFileEnv1: fd: " << fd << std::endl;
//     std::unique_lock<std::shared_timed_mutex> lock(fd_map_mutex);
//     fd_map[fd] = std::make_unique<PmemFileEnv>(fd, pathname);
//     return fd;
// }

// uint32_t CreateFileEnv(int fd) {
//     // std::cout << "CreateFileEnv2: fd: " << fd << std::endl;
//     std::unique_lock<std::shared_timed_mutex> lock(fd_map_mutex);
//     fd_map[fd] = std::make_unique<PosixFileEnv>(fd);
//     return fd;
// }


// uint32_t CreateWalReadFileEnv(int fd, const char *pathname) {
//     // std::cout << "CreateWalReadFileEnv: fd: " << fd << std::endl;
//     std::unique_lock<std::shared_timed_mutex> lock(fd_map_mutex);
//     fd_map[fd] = std::make_unique<ReadableWalFileEnv>(fd, pathname);
//     return fd;
// }

// uint32_t CreateWalWriteFileEnv(int fd, const char *pathname) {
// //     std::cout << "CreateWalWriteFileEnv: fd: " << fd << std::endl;
//     std::unique_lock<std::shared_timed_mutex> lock(fd_map_mutex);
//     fd_map[fd] = std::make_unique<WritableWalFileEnv>(fd, pathname);
//     return fd;
// }


// void RemoveFileEnv(int fd) {
//     // std::cout << "RemoveFileEnv: fd: " << fd << std::endl;
//     std::unique_lock<std::shared_timed_mutex> lock(fd_map_mutex);
//     fd_map.erase(fd);
// }




#endif  // FILE_ENV_CRUD_H
