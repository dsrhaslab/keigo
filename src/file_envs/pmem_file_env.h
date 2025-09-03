#ifndef PMEM_FILE_ENV_H
#define PMEM_FILE_ENV_H

#include <fcntl.h>
#include <unistd.h>
#include "file_env.h"

#include "../global_counters.h"

#include "../global.h"

#include "../cache_node.h"
#include "../caching_system.h"


#include "../tiering/tiering.h"

// Roundup x to a multiple of y
static size_t Roundup(size_t x, size_t y) { return ((x + y - 1) / y) * y; }
const size_t page_size = getpagesize();
size_t map_size_ = Roundup(64 << 20 /*64MB*/, page_size);

class PmemFileEnv : public FileEnv {
   public:
    bool isWritable = false;
    size_t mapped_len_;  // the length of the mapped file
    uint64_t file_offset_;
    uint8_t *pmemaddr_;  // the base address of data
    std::string fname_;
    off_t cur_offset;
    Node_c* filenode;

    int sst_number;

    std::shared_ptr<Device> device = nullptr;

    bool isACachedFile = false;

    bool other = false;



    PmemFileEnv(int fd, std::string fname, bool isACachedFile_) : FileEnv(fd), fname_(fname), isACachedFile(isACachedFile_) {

        //get sst_number from fname (get filename from fname - filepath), remove .sst and convert to int
        std::string sst_number_str = fname.substr(fname.find_last_of("/") + 1);
        sst_number_str = sst_number_str.substr(0, sst_number_str.find_last_of("."));
        sst_number = std::stoi(sst_number_str);

        mapped_len_ = 0;
        file_offset_ = 0;


        #ifdef PROFILER_3000

        add_sst_hit_entry(sst_number);

        #endif  // PROFILER_3000

        other = isACachedFile;

    }


    PmemFileEnv(int fd, std::string fname, bool isACachedFile_, bool other_) : FileEnv(fd), fname_(fname), isACachedFile(isACachedFile_) {

        //get sst_number from fname (get filename from fname - filepath), remove .sst and convert to int
        std::string sst_number_str = fname.substr(fname.find_last_of("/") + 1);
        sst_number_str = sst_number_str.substr(0, sst_number_str.find_last_of("."));
        sst_number = std::stoi(sst_number_str);

        mapped_len_ = 0;
        file_offset_ = 0;

            device = getDevice(sst_number);
            if (!isACachedFile) {
                if (device->cache != nullptr) {
                    device->cache->add_non_cached_files_access_counter(sst_number, filenode);
                }
            }
        
        // add_sst_file_number_to_string_list_map(sst_number);
        int is_pmem;
            // map the entire file
            pmemaddr_ = (uint8_t *)pmem_map_file(fname.c_str(), 0, PMEM_FILE_EXCL, 0,
                                                 &mapped_len_, &is_pmem);
            if (pmemaddr_ == NULL) {
                std::cout << "pmem_error2: " << pmem_errormsg() << std::endl;
                throw std::runtime_error(
                    std::string("pmem_map_file failed1: ").append(fname));
            }

            assert(is_pmem == true);


        other = true;

        #ifdef PROFILER_3000

        add_sst_hit_entry(sst_number);

        #endif  // PROFILER_3000

    }




    ~PmemFileEnv() {

        #ifdef PROFILER_3000
        remove_sst_hit(sst_number);
        #endif  // PROFILER_3000

        // _close(fd);
    }

    void force_destruct() {
        _close(fd, true);
    }

    int UnmapCurrentRegion() {
        pmem_unmap(pmemaddr_, mapped_len_);
        return 0;
    }

    int MapNewRegion(const bool remap) {
        // add_sstwrite_mapnewregion_counter();

        if (remap) {
            UnmapCurrentRegion();
        }

        int is_pmem;

        pmemaddr_ = (uint8_t *)pmem_map_file(fname_.c_str(), file_offset_ + map_size_,
                                             PMEM_FILE_CREATE, 0644, &mapped_len_, &is_pmem);


        if (pmemaddr_ == nullptr) {
            std::cout << "pmem_map_file error " << fname_ << std::endl;
            std::cout << "error: " << std::strerror(errno) << 
            " while remap is " << remap << '\n';
            std::cout << "pmem_error0: " << pmem_errormsg() << std::endl;
            return -1;
        }

        if (is_pmem == 0) {
            pmem_unmap(pmemaddr_, mapped_len_);
            std::cout << "is not pmem error: " << fname_ << std::endl;
            std::cout << "error: " << std::strerror(errno) << std::endl;
            std::cout << "pmem_error1: " << pmem_errormsg() << std::endl;
            return -1;
        }

        return -1;
    }

    int _open(const char *pathname, int flags) override {


        #ifdef PROFILER_3000
        device = getDevice(sst_number);
        if (!isACachedFile) {
            std::ostringstream oss;
            oss << "o " << sst_number << " " << device->name << " l" <<get_sst_level(sst_number) << std::endl;
            writePROFILING(oss.str());
        }
        #endif  // PROFILER_3000


        // check if the flag O_CREAT, O_APPEND or O_TRUNC is set
        if (flags & O_CREAT || flags & O_APPEND || flags & O_TRUNC || flags & O_WRONLY || flags & O_RDWR) {
            // add_sstwrite_constructor_counter();

            isWritable = true;
            MapNewRegion(false);
        } else {



            device = getDevice(sst_number);
            if (!isACachedFile) {
                if (device->cache != nullptr) {
                    device->cache->add_non_cached_files_access_counter(sst_number, filenode);
                }
            }

            

            int is_pmem;
            // map the entire file
            pmemaddr_ = (uint8_t *)pmem_map_file(pathname, 0, PMEM_FILE_EXCL, 0,
                                                 &mapped_len_, &is_pmem);
            if (pmemaddr_ == NULL) {
                std::cout << "pmem_error2: " << pmem_errormsg() << std::endl;
                throw std::runtime_error(
                    std::string("pmem_map_file failed1: ").append(pathname));
            }
            assert(is_pmem == true);
        }
        return fd;
    }

    int _open(const char *pathname, int flags, mode_t mode) override {

        #ifdef PROFILER_3000
        device = getDevice(sst_number);
        if (!isACachedFile) {
            std::ostringstream oss;
            oss << "o " << sst_number << " " << device->name << " l" <<get_sst_level(sst_number) << std::endl;
            writePROFILING(oss.str());
        }
        #endif  // PROFILER_3000


        // check if the flag O_CREAT, O_APPEND or O_TRUNC is set
        if (flags & O_CREAT || flags & O_APPEND || flags & O_TRUNC || flags & O_WRONLY || flags & O_RDWR) {
            
            isWritable = true;
            MapNewRegion(false);
        } else {

            device = getDevice(sst_number);
            if (!isACachedFile) {
                if (device->cache != nullptr) {
                    device->cache->add_non_cached_files_access_counter(sst_number, filenode);
                }
            }




            int is_pmem;
            // map the entire file
            pmemaddr_ = (uint8_t *)pmem_map_file(pathname, 0, PMEM_FILE_EXCL, 0,
                                                 &mapped_len_, &is_pmem);
            if (pmemaddr_ == NULL) {
                std::cout << "pmem_error3: " << pmem_errormsg() << std::endl;
                throw std::runtime_error(
                    std::string("pmem_map_file failed2: ").append(pathname));
            }

            assert(is_pmem == true);
        }
        return fd;
    }

    int _close(int fd_, bool isResetFileEnv) override {
        std::cout << "pmem-closing file: " << fname_ << " with fd " << fd << std::endl;

        //get filename from filepath (fname_)
        std::string filename = fname_.substr(fname_.find_last_of("/\\") + 1);
        //remove file extension
        filename = filename.substr(0, filename.find_last_of(".")); 

        if (!isResetFileEnv) {
            remove_sst_file(filename);
            std::cout << "removed sst file: " << filename << std::endl;

        }

        const size_t unused = mapped_len_ - file_offset_;
        pmem_unmap(pmemaddr_, mapped_len_);

        if (isWritable) {
            if (unused > 0) {
                if (truncate(fname_.c_str(), file_offset_) < 0) {
                    std::cout << "Erro while ftruncating pmem mmaped file" << fname_ << std::endl;
                }
            }
        }


        pmemaddr_ = nullptr;

        if (fd > 0) {
            return close(fd);
        }
    }

    void *_mmap(void *addr, size_t length, int prot, int flags, int fd_, off_t offset) override {
        return pmemaddr_;
    }

    off_t _lseek(int fd_, off_t offset, int whence) override {
        cur_offset = lseek(fd, offset, whence);
        return cur_offset;
    }

    ssize_t _read(int fd_, void *buf, size_t count) override {
        return _pread(fd, buf, count, cur_offset);
    }

    ssize_t _pread(int fd, void *buf, size_t count, off_t offset) override {

        #ifdef PROFILER_3000

        add_sst_hit(sst_number);

        #endif  // PROFILER_3000

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

        const auto len = std::min(mapped_len_ - offset, count);
        std::memcpy(buf, pmemaddr_ + offset, len);
        return len;
    }

    ssize_t _write(int fd, const void *buf, size_t count) override {

        const char *src = (char *)buf;
        size_t left = count;
        while (left > 0) {
            const size_t avail = mapped_len_ - file_offset_;
            if (avail == 0) {
                MapNewRegion(true);
            }

            const size_t n = (left <= avail) ? left : avail;
            pmem_memcpy_nodrain(pmemaddr_ + file_offset_, src, n);
            file_offset_ += n;
            src += n;
            left -= n;
        }
        return count;
    }

    int _fsync(int fd) override {

        pmem_drain();
        return 0;
    }

    int _fdatasync(int fd) override {

        pmem_drain();
        return 0;
    }

    ssize_t _readahead(int fd_, off64_t offset, size_t count) {
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
        FILE * ret = fdopen(fd, mode);
        ret->_fileno = fildes;
        return ret;
        // return fdopen(fd, mode);
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
        
    }

    int _feof(FILE *stream) {
        return 1;
    }


};

#endif  // PMEM_FILE_ENV_H

