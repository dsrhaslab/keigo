#ifndef SSTW_FILE_ENV_H
#define SSTW_FILE_ENV_H


#include <fcntl.h>
#include <unistd.h>
#include "file_env.h"
#include "pmem_file_env.h"
#include "../global_counters.h"

#include "../global.h"

#include "../tiering/tiering.h"
class SSTWriteFileEnv : public FileEnv {
   public:
    size_t mapped_len_;  // the length of the mapped file
    uint64_t file_offset_;
    uint8_t *pmemaddr_;  // the base address of data
    std::string fname_;
    off_t cur_offset;

    int sst_number;

    SSTWriteFileEnv(int fd, std::string fname) : FileEnv(fd), fname_(fname) {
        mapped_len_ = 0;
        file_offset_ = 0;

        //get sst_number from fname (get filename from fname - filepath), remove .sst and convert to int
        std::string sst_number_str = fname.substr(fname.find_last_of("/") + 1);
        sst_number_str = sst_number_str.substr(0, sst_number_str.find_last_of("."));
        sst_number = std::stoi(sst_number_str);

        // std::cout << "SSTWriteFileEnv: " << fname << std::endl;
    }

    ~SSTWriteFileEnv() {
        std::cout << "destructing sst write file" << std::endl;

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

        // if(remap) {
        //     std::cout << "------------------------------" << std::endl;
        //     std::cout << "MapNewRegion: fname_:" << fname_ << std::endl;
        //     std::cout << "MapNewRegion: file_offset_: " << file_offset_ << std::endl;
        //     std::cout << "MapNewRegion: map_size_: " << map_size_ << std::endl;
        //     std::cout << "MapNewRegion: mapped_len_: " << mapped_len_ << std::endl;
        // }


        if (pmemaddr_ == nullptr) {
            std::cout << "pmem_map_file error " << fname_ << std::endl;
            std::cout << "error: " << std::strerror(errno) << 
            " while remap is " << remap << '\n';
            // std::cout << "disk space: " << std::endl;
            // system("df -h /dev/pmem0");
            std::cout << "pmem_error4: " << pmem_errormsg() << std::endl;
            return -1;
        }

        if (is_pmem == 0) {
            pmem_unmap(pmemaddr_, mapped_len_);
            std::cout << "is not pmem error: " << fname_ << std::endl;
            std::cout << "error: " << std::strerror(errno) << std::endl;
            std::cout << "pmem_error5: " << pmem_errormsg() << std::endl;
            return -1;
        }

        return -1;
    }

    int _open(const char *pathname, int flags) override {

        writer_threads_num++;

        // std::cout << "opening sst write file: " << fname_ << std::endl;

        MapNewRegion(false);
        return fd;
    }

    int _open(const char *pathname, int flags, mode_t mode) override {
        // std::cout << "opening sst write file2: " << fname_ << std::endl;
        writer_threads_num++;


        MapNewRegion(false);
        return fd;
    }

    int _close(int fd_, bool isResetFileEnv) override {

        writer_threads_num--;


        // std::cout open-pmem2:<< "closing sst write file: " << fname_ << std::endl;
        std::cout << "closing file: " << fname_ << " with fd " << fd << std::endl;


        #ifdef PROFILER_3000
        std::shared_ptr<Device> device = getDevice(sst_number);
        std::ostringstream oss;
        oss << "o " << sst_number << " " << device->name << " l" <<get_sst_level(sst_number) << std::endl;
        writePROFILING(oss.str());
        #endif  // PROFILER_3000


        //get filename from filepath (fname_)
        std::string filename = fname_.substr(fname_.find_last_of("/\\") + 1);
        //remove file extension
        filename = filename.substr(0, filename.find_last_of(".")); 

        if (!isResetFileEnv) {
            remove_sst_file(filename);
            std::cout << "removed sst file: " << filename << std::endl;

        }


        // add_sstwrite_close_counter();

        const size_t unused = mapped_len_ - file_offset_;
        pmem_unmap(pmemaddr_, mapped_len_);

        if (unused > 0) {
            if (truncate(fname_.c_str(), file_offset_) < 0) {
                std::cout << "Erro while ftruncating pmem mmaped file" << fname_ << std::endl;
            }
        }

        pmemaddr_ = nullptr;

        // closes the real fd
        // return close(fd);
        return 0;
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
        // std::cout << "pmem-pread: start: " << offset << " " << count << std::endl;
        const auto len = std::min(mapped_len_ - offset, count);
        std::memcpy(buf, pmemaddr_ + offset, len);
        // std::cout << "pmem-pread: end: " << offset << " " << count << std::endl;
        return len;
    }

    ssize_t _write(int fd, const void *buf, size_t count) override {
        // add_sstwrite_append_counter();

        const char *src = (char *)buf;
        size_t left = count;
        while (left > 0) {
            const size_t avail = mapped_len_ - file_offset_;
            if (avail == 0) {
                MapNewRegion(true);
            }

            const size_t n = (left <= avail) ? left : avail;
            pmem_memcpy_nodrain(pmemaddr_ + file_offset_, src, n);
            // pmem_memcpy_persist(base_ + file_offset_, src, n);
            file_offset_ += n;
            src += n;
            left -= n;
        }
        return count;
    }

    int _fsync(int fd) override {
        // add_sstwrite_fsync_counter();

        pmem_drain();
        return 0;
    }

    int _fdatasync(int fd) override {
        // add_sstwrite_sync_counter();

        pmem_drain();
        return 0;
    }

    ssize_t _readahead(int fd_, off64_t offset, size_t count) {
        // std::cout << "readahead" << std::endl;
        return 0;
    }
    
    int _fcntl(int fd_, int cmd, ... /* arg */ ) {
        // std::cout << "fcntl" << std::endl;
        return 0;
    }
    
    int _posix_fadvise(int fd_, off_t offset, off_t len, int advice) {
        // std::cout << "posix_fadvise" << std::endl;
        return 0;
    }

    ssize_t _pwrite(int fd_, const void *buf, size_t count, off_t offset) {
        // std::cout << "pwrite" << std::endl;
        return 0;
    }

    int _ftruncate(int fd_, off_t length) {
        // std::cout << "ftruncate" << std::endl;
        return 0;
    }

    int _fallocate(int fd_, int mode, off_t offset, off_t len) {
        // std::cout << "fallocate" << std::endl;
        return 0;
    }

    int _sync_file_range(int fd_, off64_t offset, off64_t nbytes, unsigned int flags) {
        // std::cout << "sync_file_range for: " << fname_ << std::endl;
        return 0;
    }

    int _fstat(int fd_, struct stat *buf) {
        // std::cout << "fstat" << std::endl;
        return 0;
    }

    FILE *_fdopen(int fildes, const char *mode) {
        // std::cout << "fdopen" << std::endl;
        return nullptr;
    }

    size_t _fread_unlocked(void *ptr, size_t size, size_t n, FILE *stream) {
        // std::cout << "fread_unlocked" << std::endl;
        return 0;
    }

    int _fseek(FILE *stream, long offset, int whence) {
        // std::cout << "fseek" << std::endl;
        return 0;
    }

    int _fclose(FILE *stream) {
        // std::cout << "fclose" << std::endl;
        return 0;
    }

    void _clearerr(FILE *stream) {
        // std::cout << "clearerr" << std::endl;
        
    }

    int _feof(FILE *stream) {
        // std::cout << "feof" << std::endl;
        return 1;
    }


};


#endif  // PMEM_FILE_ENV_H
