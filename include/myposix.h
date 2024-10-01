#ifndef MY_POSIX_H
#define MY_POSIX_H

#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <memory>
#include <vector>
#include <thread>
#include <map>
#include <mutex>
#include <string>
#include <list>
#include <shared_mutex>
#include <vector>
#include <queue>
#include <thread>
#include <atomic>

// Structures

enum op_type {
  THREAD_COMP_L1 = 1,
  THREAD_COMP_L2 = 2,
  THREAD_COMP_L3 = 3,
  THREAD_COMP_L4 = 4,
  THREAD_COMP_L5 = 5,
  THREAD_COMP_L6 = 6,
  THREAD_COMP_L7 = 7,
  THREAD_COMP_L8 = 8,
  THREAD_COMP_L9 = 9,
  THREAD_COMP_L10 = 10,
  THREAD_FLUSH = 0,
  THREAD_OTHER = -1
};

enum device_type {
  SSD = 0,
  NVMM = 1,
  NONE = -1
};

enum FileAccessType {
    SST_Read = 0,
    SST_Write = 1,
    WAL_Read = 2,
    WAL_Write = 3,
    ACCESSOR_OTHER = 4,
};

enum copy_type {
  NVMM_TO_SSD = 0,
  SSD_TO_NVMM = 1,
  NO_COPY = -1
};

class Context {
   public:
    FileAccessType type_;
    bool is_pmem_;
    Context(FileAccessType type, bool is_pmem) : 
        type_(type), is_pmem_(is_pmem) {}
};

void add_sst_level(int sst_file_number, int level);

bool getFileActualPathOpen(std::string fname, std::string &actualPath_ret, std::shared_ptr<Context>& context);
std::string getFileActualPath(std::string fname);
std::string getFileActualPathUnlink(std::string fname, std::shared_ptr<Context>& context);

void activate();
bool getRunNow();


void* caching_thread(void *ptr);
void* caching_thread_c(void *ptr);
void* check_hitratio(void* ptr);

void stopCacheCondition();

// main api

void init_tiering_lib();
void end_tiering_lib();

void registerThread(pthread_t thread_id, op_type type);
void registerStartCompaction(pthread_t compaction_id, int new_level);


// trivial moves api
void enqueueTrivialMove(std::string filename, int input_level, int output_level);

//auxiliary functions

int isSSTorWal(std::string filename);
op_type getThreadType(pthread_t thread_id);

// POSIX

int my_unlink(const char *pathname, std::shared_ptr<Context> ctx);
int my_open(const char *pathname, int flags, std::shared_ptr<Context> ctx);
int my_open(const char *pathname, int flags, mode_t mode, std::shared_ptr<Context> ctx);
int my_close(int fd);
void *my_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
off_t my_lseek(int fd, off_t offset, int whence);
ssize_t my_read(int fd, void *buf, size_t count);
ssize_t my_pread(int fd, void *buf, size_t count, off_t offset);
ssize_t my_write(int fd, const void *buf, size_t count);
int my_fsync(int fd);
int my_fdatasync(int fd);
ssize_t my_readahead(int fd, off64_t offset, size_t count);
int my_fcntl(int fd, int cmd, ... /* arg */);
int my_posix_fadvise(int fd, off_t offset, off_t len, int advice);
ssize_t my_pwrite(int fd, const void *buf, size_t count, off_t offset);
int my_ftruncate(int fd, off_t length);
int my_fallocate(int fd, int mode, off_t offset, off_t len);
int my_sync_file_range(int fd, off64_t offset, off64_t nbytes, unsigned int flags);
int my_fstat(int fd, struct stat *buf);
size_t my_fread_unlocked(void *ptr, size_t size, size_t n, FILE *stream);
int my_fseek(FILE *stream, long offset, int whence);
int my_fclose(FILE *stream);
FILE *my_fdopen(int fildes, const char *mode);
int my_feof(FILE *stream);
void my_clearerr(FILE *stream);

#endif  // MY_POSIX_H