#ifndef POSIX_FILE_ENV_H
#define POSIX_FILE_ENV_H

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
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>

#include "file_env.h"

#include "../global.h"
#include "../tiering/tiering.h"

#include "../cache_node.h"
#include "../caching_system.h"

class PosixFileEnv : public FileEnv {
   public:
    std::string fname_;
    void *base_addr_;

    int sst_number;
    bool cached;
    size_t size;
    // Node* filenode;
    Node_c* filenode;
    bool isREADONLY = false;

    std::shared_ptr<Device> device = nullptr;

    bool isACachedFile = false;

    std::string extension = "";



    PosixFileEnv(int fd, std::string fname, bool isACachedFile_) : FileEnv(fd), fname_(fname), isACachedFile(isACachedFile_) {
        //calculate this fnames extension
        extension = fname.substr(fname.find_last_of(".") + 1);

        //get sst_number from fname (get filename from fname - filepath), remove .sst and convert to int
        std::string sst_number_str = fname.substr(fname.find_last_of("/") + 1);
        sst_number_str = sst_number_str.substr(0, sst_number_str.find_last_of("."));
        sst_number = std::stoi(sst_number_str);

        // add_sst_file_number_to_string_list_map(sst_number);

        // std::cout << "PosixFileEnv: " << fname << std::endl;



        #ifdef PROFILER_3000

        add_sst_hit_entry(sst_number);

        #endif  // PROFILER_3000


    }

    ~PosixFileEnv() {
        #ifdef PROFILER_3000
            if (extension != "log") {

        remove_sst_hit(sst_number);

            }
        #endif  // PROFILER_3000


        // std::cout << "PosixFileEnv destructor" << std::endl;
        // std::cout << "closing file1: " << fname_ << " with fd " << fd << std::endl;

        // _close(fd);
    }

    void force_destruct() {
        // std::cout << "closing file2: " << fname_ << " with fd " << fd << std::endl;

         _close(fd, true);
    }

    int _open(const char *pathname, int flags) override {

        // std::cout << "open-posix1: " << pathname << " | fd is: " << fd << " by " << pthread_self() << std::endl;
        // std::cout << "open: " << sst_number << " | level: " << get_sst_level(sst_number) << " - "<< std::endl;

        // std::stringstream ss;

        // std::string currentTime = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        // ss << "open: " << sst_number << " | level: " << get_sst_level(sst_number) << " - " <<  " | " << currentTime << std::endl;
        // print_to_screen(ss.str());
        
        // device = getDevice(sst_number);
        // if (!isACachedFile) {
        //     if (device->lru_working) {
        //         device->lru.add(sst_number);
        //     }
        // }
        
        #ifdef PROFILER_3000
            if (extension != "log") {

        device = getDevice(sst_number);
        if (!isACachedFile) {
            std::ostringstream oss;
            oss << "o " << sst_number << " " << device->name << " l" <<get_sst_level(sst_number) << std::endl;
            writePROFILING(oss.str());
        }
            }
        #endif  // PROFILER_3000

        if (flags & O_CREAT || flags & O_APPEND || flags & O_TRUNC || flags & O_WRONLY || flags & O_RDWR) {
        } else {
            // std::cout << "open-read1: " << pathname << " | fd is: " << fd << " flags are " << flags << " by " << pthread_self() << std::endl;


            isREADONLY = true;
            // Node* newNode = new Node(sst_number, 1);
            filenode = new Node_c(sst_number, 1);

            // std::cout << "newNode: " << newNode << std::endl;
            // filenode = newNode;

            // int level = get_sst_level(sst_number);
            // if (level >= 0) {
            //     device = getDeviceFromLevel(level);
            //     if (device->cache != nullptr) {
            //         device->cache->add_non_cached_files_access_counter(sst_number, filenode);
            //         std::cout << "added to cache" << std::endl;
            //     }
            // }

            if (extension != "log") {
                device = getDevice(sst_number);

                if (!isACachedFile) {

                    // if (device->lru_working) {
                    //     device->lru.add(sst_number);
                    // }

                    // std::cout << "adding to lru: " << sst_number << std::endl;
                    if (device->lru_working) {


                        // struct stat stat_buf;
        
                        // if (stat(fname_.c_str(), &stat_buf) == 0) {
                        //     std::cout << "File2 size of " << fname_.c_str() << " is " << stat_buf.st_size << " bytes.\n";
                        // } else {
                        //     std::cerr << "Error: Could not get file statistics.\n";

                        // }

                        device->lru->add(sst_number);
                    }

                    if (device->cache != nullptr) {
                        device->cache->add_non_cached_files_access_counter(sst_number, filenode);
                        // std::cout << "added to cache" << std::endl;
                    }
                }
            }
            



            // if (get_sst_level(sst_number) == 3 || get_sst_level(sst_number) == 4) {
                // add_non_cached_files_access_counter_c(sst_number, filenode);
                // std::cout << "add_non_cached_files_access_counter_c: " << sst_number << std::endl;
                // add_non_cached_files_access_counter(sst_number, newNode);
            // }
        }

        cached = false;

        // Node* newNode = new Node(sst_number, 1);
        // // std::cout << "newNode: " << newNode << std::endl;
        // filenode = newNode;

        // add_non_cached_files_access_counter(sst_number, newNode);

        // cached = false;

        return fd;
    }

    int _open(const char *pathname, int flags, mode_t mode) override {

        // std::cout << "open-posix2: " << pathname << std::endl;

        // std::stringstream ss;

        // std::string currentTime = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());        

        // ss << "open: " << sst_number << " | level: " << get_sst_level(sst_number) << " - " <<  " | " << currentTime << std::endl;
        // print_to_screen(ss.str());

        // std::cout << "open: " << sst_number << " | level: " << get_sst_level(sst_number) << " - " << std::endl;

        // device = getDevice(sst_number);
        // if (!isACachedFile) {
        //     if (device->lru_working) {
        //         device->lru.add(sst_number);
        //     }
        // }




        #ifdef PROFILER_3000
            if (extension != "log") {

                device = getDevice(sst_number);
                if (!isACachedFile) {
                    std::ostringstream oss;
                    oss << "o " << sst_number << " " << device->name << " l" <<get_sst_level(sst_number) << std::endl;
                    writePROFILING(oss.str());
                }
            }
        #endif  // PROFILER_3000


        if (flags & O_CREAT || flags & O_APPEND || flags & O_TRUNC || flags & O_WRONLY || flags & O_RDWR) {
        } else {
            // std::cout << "open-read2: " << pathname << " | fd is: " << fd << " flags are " << flags << " mode is "  << mode << " by " << pthread_self() << std::endl;

            isREADONLY = true;
            
            // Node* newNode = new Node(sst_number, 1);
            filenode = new Node_c(sst_number, 1);

        
            // int level = get_sst_level(sst_number);
            // if (level >= 0) {
            //     device = getDeviceFromLevel(level);
            //     if (device->cache != nullptr) {
            //         device->cache->add_non_cached_files_access_counter(sst_number, filenode);
            //         std::cout << "added to cache" << std::endl;
            //     }
            // }



            if (extension != "log") {

                device = getDevice(sst_number);
                if (!isACachedFile) {






                    // std::cout << "adding to lru: " << sst_number << std::endl;
                    if (device->lru_working) {

                        // struct stat stat_buf;
        
                        // if (stat(fname_.c_str(), &stat_buf) == 0) {
                        //     std::cout << "File2 size of " << fname_.c_str() << " is " << stat_buf.st_size << " bytes.\n";
                        // } else {
                        //     std::cerr << "Error: Could not get file statistics.\n";

                        // }

                        device->lru->add(sst_number);
                    }
                
                    // if (device->lru_working) {
                    //     device->lru.add(sst_number);
                    // }

                    if (device->cache != nullptr) {
                        device->cache->add_non_cached_files_access_counter(sst_number, filenode);
                        // std::cout << "added to cache" << std::endl;
                    }
                }
            }


            // if (get_sst_level(sst_number) == 3 || get_sst_level(sst_number) == 4) {
                // add_non_cached_files_access_counter_c(sst_number, filenode);
                // std::cout << "add_non_cached_files_access_counter_c: " << sst_number << std::endl;
                // add_non_cached_files_access_counter(sst_number, newNode);

            // }
        }

        cached = false;

        // Node* newNode = new Node(sst_number, 1);
        // // std::cout << "newNode: " << newNode << std::endl;
        // filenode = newNode;

        // add_non_cached_files_access_counter(sst_number, newNode);

        // cached = false;

        return fd;
    }

    int _close(int fd_, bool isResetFileEnv) override {
        std::cout << "posix-closing file: " << fname_ << " with fd " << fd << std::endl;

        if (extension != "log") {

            if (!isREADONLY) {
                device = getDevice(sst_number);
                if (!isACachedFile) {
                    // std::cout << "adding to lru: " << sst_number << std::endl;
                    if (device->lru_working) {
                        // device->lru->add(sst_number);
                    }
                }
            }



            // add_sst_file_number_to_size_map(sst_number, size);

            //get filename from filepath (fname_)
            std::string filename = fname_.substr(fname_.find_last_of("/\\") + 1);
            //remove file extension
            filename = filename.substr(0, filename.find_last_of(".")); 

            if (!isResetFileEnv) {
                remove_sst_file(filename);
                std::cout << "removed sst file: " << filename << std::endl;
            }
        }
        // closes the real fd
        // std::cout << "closing posix fd: " << fd << std::endl;
        return close(fd);
    }

    void *_mmap(void *addr, size_t length, int prot, int flags, int fd_, off_t offset) override {
        return mmap(addr, length, prot, flags, fd, offset);
    }

    off_t _lseek(int fd_, off_t offset, int whence) override {
        return lseek(fd, offset, whence);
    }

    ssize_t _read(int fd_, void *buf, size_t count) override {

        #ifdef PROFILER_3000

        add_sst_hit(sst_number);

        #endif  // PROFILER_3000

        // global_posix_counter++;
            if (extension != "log") {

        if (device != nullptr) {
            if (device->cache != nullptr) {
                device->cache->storage_access_counter++;
            }
            if (device->next != nullptr) {
                if (device->next->cache != nullptr) {
                    device->next->cache->cache_access_counter++;
                }
            }
        }

            }

        // increment_nvme_access_counter();

        if (isREADONLY) {
            filenode->addOne();
        }

        // std::stringstream ss;
        // std::string currentTime = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());        
        // ss << "read: " << sst_number << " | level: " << get_sst_level(sst_number) << " - " <<  " | " << currentTime << std::endl;
        // print_to_screen(ss.str());

        // add_string_to_sst_file_number_to_string_list_map(sst_number);

        
        // filenode->turnLastZeroToOne();
        return read(fd, buf, count);
    }


    ssize_t _pread(int fd_, void *buf, size_t count, off_t offset) override {

        #ifdef PROFILER_3000

        add_sst_hit(sst_number);

        #endif  // PROFILER_3000

        // global_posix_counter++;

            if (extension != "log") {

        if (device != nullptr) {
            if (device->cache != nullptr) {
                device->cache->storage_access_counter++;
            }
            if (device->next != nullptr) {
                if (device->next->cache != nullptr) {
                    device->next->cache->cache_access_counter++;
                }
            }
        }

            }

        // increment_nvme_access_counter();

        if (isREADONLY) {
            filenode->addOne();
        }


        // std::stringstream ss;
        // std::string currentTime = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());        
        // ss << "read: " << sst_number << " | level: " << get_sst_level(sst_number) << " - " <<  " | " << currentTime << std::endl;
        // print_to_screen(ss.str());

        // add_string_to_sst_file_number_to_string_list_map(sst_number);


        // filenode->turnLastZeroToOne();
        return pread(fd, buf, count, offset);
    }

    ssize_t _write(int fd_, const void *buf, size_t count) override {
        // size += count;
        ssize_t done = write(fd, buf, count);
        if (done < 0) {
            std::cout << "write failed: " << " filename: " << fname_ << " fd: " << fd << " " << strerror(errno) << std::endl;
        }
        return done;
    }

    int _fsync(int fd_) override {
        return fsync(fd);
    }

    int _fdatasync(int fd_) override {
        return fdatasync(fd);
    }

    ssize_t _readahead(int fd_, off64_t offset, size_t count) {
        // std::cout << "readahead: " << sst_number << " offset: " << offset << " count: " << count << std::endl;
        return readahead(fd, offset, count);
    }
    
    int _fcntl(int fd_, int cmd, ... /* arg */ ) {
        // call the original fcntl function with the same arguments
        va_list args;
        va_start(args, cmd);
        void *arg = va_arg(args, void *);
        va_end(args);

        return fcntl(fd, cmd, arg);
    }
    
    int _posix_fadvise(int fd_, off_t offset, off_t len, int advice) {
        return posix_fadvise(fd, offset, len, advice);
    }

    ssize_t _pwrite(int fd_, const void *buf, size_t count, off_t offset) {
        return pwrite(fd, buf, count, offset);
    }

    int _ftruncate(int fd_, off_t length) {
        return ftruncate(fd, length);
    }

    int _fallocate(int fd_, int mode, off_t offset, off_t len) {
        return fallocate(fd, mode, offset, len);
    }

    int _sync_file_range(int fd_, off64_t offset, off64_t nbytes, unsigned int flags) {
        return sync_file_range(fd, offset, nbytes, flags);
    }

    int _fstat(int fd_, struct stat *buf) {
        return fstat(fd, buf);
    }

    FILE *_fdopen(int fildes, const char *mode) {
        return fdopen(fd, mode);
    }

    size_t _fread_unlocked(void *ptr, size_t size, size_t n, FILE *stream) {
        //create a new file stream with the real fd. Return and close the new stream
        FILE *new_stream = fdopen(fd, "r");
        size_t ret = fread_unlocked(ptr, size, n, new_stream);
        fclose(new_stream);
        return ret;
    }

    int _fseek(FILE *stream, long offset, int whence) {
        //create a new file stream with the real fd. Return and close the new stream
        FILE *new_stream = fdopen(fd, "r");
        int ret = fseek(new_stream, offset, whence);
        fclose(new_stream);
        return ret;
    }

    int _fclose(FILE *stream) {
        //create a new file stream with the real fd. Return and close the new stream
        FILE *new_stream = fdopen(fd, "r");
        int ret = fclose(new_stream);
        return ret;
    }

    void _clearerr(FILE *stream) {
        //create a new file stream with the real fd. Return and close the new stream
        FILE *new_stream = fdopen(fd, "r");
        clearerr(new_stream);
    }

    int _feof(FILE *stream) {
        //create a new file stream with the real fd. Return and close the new stream
        FILE *new_stream = fdopen(fd, "r");
        int ret = feof(new_stream);
        return ret;
    }
};

#endif  // POSIX_FILE_ENV_H