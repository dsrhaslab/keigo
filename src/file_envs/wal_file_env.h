#ifndef WAL_FILE_ENV_H
#define WAL_FILE_ENV_H

#include <libpmem.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <stack>
#include <vector>

#include "file_env.h"
#include "pmem_file_env.h"

#include "../global_counters.h"


class ReadableWalFileEnv : public PmemFileEnv {
   private:
    size_t file_size_;    // the length of the whole file
    uint8_t *data_;       // the base address of data
    size_t data_length_;  // the length of data
    size_t seek_;         // next offset to read

   public:
    ReadableWalFileEnv(int fd, std::string fname) : PmemFileEnv(fd, fname, false) {
        add_walread_constructor_counter();

        // get the file size
        struct stat stat;
        if (fstat(fd, &stat) < 0) {
            close(fd);
            throw std::runtime_error(std::string("fstat '")
                                         .append(fname)
                                         .append("' failed: ")
                                         .append(strerror(errno)));
        }
        file_size_ = stat.st_size;
        // whole file
        void *base = mmap(0, file_size_, PROT_READ, MAP_SHARED, fd, 0);
        // no matter map ok or fail, we can close now
        // V2: cant close beacuse fdopen needs it
        // close(fd);

        if (base == MAP_FAILED) {
            throw std::runtime_error(
                std::string("mmap file failed: ").append(strerror(errno)));
        }

        data_length_ = *((size_t *)base);
        data_ = (uint8_t *)base + sizeof(size_t);
        seek_ = 0;
    }

    // already closed it in constructor
    // V2: nevermind 
    int _close(int fd_, bool isResetFileEnv) override {
        add_walread_destructor_counter();

        void* base = (void*)(data_ - sizeof(size_t));
        munmap(base, file_size_);
        close(fd);

        return 0;
    }

    // already closed fd in constructor
    int _fclose(FILE *stream) override {
        add_walread_destructor_counter();

        void* base = (void*)(data_ - sizeof(size_t));
        munmap(base, file_size_);

        close(fd);
        return 0;
    }

    int _open(const char *pathname, int flags) override {
        return fd;
    }

    int _open(const char *pathname, int flags, mode_t mode) override {
        return fd;
    }

    // careful with size and n
    size_t _fread_unlocked(void *ptr, size_t size, size_t n, FILE *stream) override {

        add_walread_read_counter();

        // assume size == 1
        auto len = std::min(data_length_ - seek_, n);
        memcpy(ptr, (char *)data_ + seek_, len);
        // *result = Slice((char*)data_ + seek_, len);
        seek_ += len;
        return len;
    }


    int _fseek(FILE *stream, long offset, int whence) {
        add_walread_skip_counter();

        // assume whence is SEEK_CUR
        assert(seek_ <= data_length_);
        auto len = std::min(data_length_ - seek_, (uint64_t)offset);
        seek_ += len;
        return 0;
    }
};

// the initial allocation size of the log file
const size_t wal_init_size = 256 << 20;  // 256 MiB
// expand the file with this size every time it is not enough
const size_t wal_size_addition = 64 << 20;  // 64 MiB

class WritableWalFileEnv : public PmemFileEnv {
   private:
    size_t size_addition_;  // expand size_addition bytes
                            // every time left space is not enough
    size_t data_length_;    // the writen data length
    size_t file_size_;      // the length of the whole file
    size_t *p_length_;      // the first size_t to record data length
    void *map_base_;        // mmap()-ed area
    size_t map_length_;     // mmap()-ed length
    uint8_t *buffer_;       // the base address of buffer to write

    // this flag strictly requires the file system to be on DCPMM
    static const int MMAP_FLAGS = MAP_SHARED_VALIDATE | MAP_SYNC;

    // memcpy non-temporally
    static void MemcpyNT(void *dst, const void *src, size_t len) {
        pmem_memcpy(dst, src, len, PMEM_F_MEM_NONTEMPORAL);
        ////////pmem_memcpy(dst, src, len, PMEM_F_MEM_TEMPORAL);
    }

    // set a 64-bit non-temporally
    static void SetSizeNT(size_t *dst, size_t value) {
        assert(sizeof(size_t) == 8);
        assert((size_t)dst % sizeof(size_t) == 0);
        __builtin_ia32_movnti64((long long *)dst, value);
    }

   public:
    WritableWalFileEnv(int fd, std::string fname) : PmemFileEnv(fd, fname, false),
        size_addition_(wal_size_addition),
        data_length_(0),
        file_size_(0),
        p_length_(nullptr),
        map_base_(nullptr),
        map_length_(0),
        buffer_(nullptr) {

        add_walwrite_constructor_counter();

            
        if (wal_init_size < sizeof(size_t) || wal_size_addition == 0) {
            throw std::runtime_error("init_size too small or size_addition is zero");
        }

        // pre-allocate space
        if (fallocate(fd, 0, 0, wal_init_size) != 0) {
            close(fd);
            throw std::runtime_error(std::string("fallocate '")
                                         .append(fname)
                                         .append("' failed: ")
                                         .append(strerror(errno)));
        }

        // first size_t is to record length of data
        void *p_length = mmap(0, sizeof(size_t), PROT_WRITE, MMAP_FLAGS, fd, 0);
        if (p_length == MAP_FAILED) {
            close(fd);
            throw std::runtime_error(
                std::string("mmap first size_t failed: ").append(strerror(errno)));
        }

        // whole file
        void *base = mmap(0, wal_init_size, PROT_WRITE, MMAP_FLAGS, fd, 0);
        if (base == MAP_FAILED) {
            close(fd);
            munmap(p_length, sizeof(size_t));
            throw std::runtime_error(
                std::string("mmap whole file failed: ").append(strerror(errno)));
        }

        size_addition_ = wal_size_addition;
        data_length_ = 0;
        file_size_ = wal_init_size;
        p_length_ = (size_t *)p_length;
        map_base_ = base;
        map_length_ = wal_init_size;
        buffer_ = (uint8_t *)base + sizeof(size_t);

        // reset the data length
        SetSizeNT(p_length_, 0);
        __builtin_ia32_sfence();
    }

    int _ftruncate(int fd_, off_t length) {
        data_length_ = length;
        SetSizeNT(p_length_, data_length_);
        __builtin_ia32_sfence();
        return 0;
    }

    int _open(const char *pathname, int flags) override {
        return fd;
    }

    int _open(const char *pathname, int flags, mode_t mode) override {
        return fd;
    }

    int _close(int fd_, bool isResetFileEnv) override {
        add_walwrite_close_counter();


        munmap(p_length_, sizeof(size_t));
        munmap(map_base_, map_length_);

        // closes the real fd
        return close(fd);
    }

    int _fsync(int fd_) override {
        return 0;
    }

    int _fdatasync(int fd_) override {
        return 0;
    }

    ssize_t _write(int fd_, const void *buf, size_t count) override {
        add_walwrite_append_counter();

        const size_t len = count;
        // the least file size to write the new slice
        const size_t least_file_size = sizeof(size_t) + data_length_ + len;
        // left space is not enough, need expand
        if (least_file_size > file_size_) {
        // the multiple of size_addition to add
        const size_t count =
            (least_file_size - file_size_ - 1) / size_addition_ + 1;
        const size_t add_size = count * size_addition_;
        // allocate new space
        if (fallocate(fd, 0, file_size_, add_size) != 0) {
            throw std::runtime_error(
                    std::string("expand file failed: ").append(strerror(errno)));
        }
        const size_t new_file_size = file_size_ + add_size;
        assert(new_file_size >= least_file_size);
        // the offset in file to mmap(), align to 4K
        const size_t offset = sizeof(size_t) + data_length_;
        const size_t offset_aligned = offset & ~(4096 - 1);
        assert(offset_aligned <= offset);
        // mmaped() the new area, but because aligned,
        // usually overlip with current mmap()-ed range
        const size_t new_map_length = new_file_size - offset_aligned;
        void* new_map_base =
            mmap(0, new_map_length, PROT_WRITE, MMAP_FLAGS, fd, offset_aligned);
        if (new_map_base == MAP_FAILED) {
            throw std::runtime_error(
                    std::string("mmap new range failed: ").append(strerror(errno)));
        }
        // unmap the previous range
        munmap(map_base_, map_length_);
        file_size_ = new_file_size;
        map_base_ = new_map_base;
        map_length_ = new_map_length;
        buffer_ = (uint8_t*)new_map_base + offset - offset_aligned - data_length_;
        }
        MemcpyNT(buffer_ + data_length_, buf, len);
        // need sfence, make sure all data is persisted to DCPMM
        __builtin_ia32_sfence();
        data_length_ += len;
        SetSizeNT(p_length_, data_length_);
        // need sfence, make sure all data is persisted to DCPMM
        __builtin_ia32_sfence();
        return count;
    
    }
};


#endif  // WAL_FILE_ENV_H