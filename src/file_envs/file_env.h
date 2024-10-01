#ifndef FILE_ENV_H
#define FILE_ENV_H

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

class FileEnv {
   public:
    int fd;
    FileEnv(int fd_) : fd(fd_) {}

    virtual void force_destruct() = 0;
    

    virtual int _open(const char *pathname, int flags) = 0;
    virtual int _open(const char *pathname, int flags, mode_t mode) = 0;
    virtual int _close(int fd, bool isResetFileEnv) = 0;
    virtual void *_mmap(void *addr, size_t length, int prot, int flags, int fd_, off_t offset) = 0;
    virtual off_t _lseek(int fd, off_t offset, int whence) = 0;
    virtual ssize_t _read(int fd, void *buf, size_t count) = 0;
    virtual ssize_t _pread(int fd, void *buf, size_t count, off_t offset) = 0;
    virtual ssize_t _write(int fd, const void *buf, size_t count) = 0;
    virtual int _fsync(int fd) = 0;
    virtual int _fdatasync(int fd) = 0;
    virtual ssize_t _readahead(int fd, off64_t offset, size_t count) = 0;
    virtual int _fcntl(int fd, int cmd, ... /* arg */ ) = 0;
    virtual int _posix_fadvise(int fd, off_t offset, off_t len, int advice) = 0;
    virtual ssize_t _pwrite(int fd, const void *buf, size_t count, off_t offset) = 0;
    virtual int _ftruncate(int fd, off_t length) = 0;
    virtual int _fallocate(int fd, int mode, off_t offset, off_t len) = 0;
    virtual int _sync_file_range(int fd, off64_t offset, off64_t nbytes, unsigned int flags) = 0;
    virtual int _fstat(int fd, struct stat *buf) = 0;

    virtual size_t _fread_unlocked(void *ptr, size_t size, size_t n, FILE *stream) = 0;
    virtual int _fseek(FILE *stream, long offset, int whence) = 0;
    virtual int _fclose(FILE *stream) = 0;
    virtual FILE *_fdopen(int fildes, const char *mode) = 0;

    virtual void _clearerr(FILE *stream) = 0;
    virtual int _feof(FILE *stream) = 0;

};

#endif  // FILE_ENV_H