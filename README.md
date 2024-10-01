
### Installation

The repo contains scripts for automatic installation:

`./build.sh build`

---
### Usage

An example of how one can use this backend can be see here:

https://github.com/dsrhaslab/tiered-rocksdb/commit/9fcbfd5abc29153d75bd0a927d74af80eee9b6e7

It shows all the modifications made to a vanilla version of RocksDB in order to use this middleware. Below is a more detailed explanation.

#### Steps

##### Initialization
The library first needs to be initialized.

```cpp
init_tiering_lib();
```

##### Register Threads
Both flush and compaction threads need to be identified.

```cpp
registerThread(pthread_self(),THREAD_FLUSH);
registerThread(pthread_self(),THREAD_COMP_L1);
```

##### Register Compactions

Whenever compactions are triggered, we need to inform the library by passing the
current thread's id and the output level of the compaction.

```cpp
registerStartCompaction(pthread_self(), compaction_level_end);
```

##### Register Trivial Moves 

If the key-value store performs trivial moves, it is important to move the file to the correct device if necessary.

```cpp
enqueueTrivialMove(sst_number, input_level, output_level);
```

##### New Posix

The new posix system calls that use the SST and WAL files need to be replaced to by the new backend system calls.

```cpp
int my_open(const char *pathname, int flags, mode_t mode, std::shared_ptr<Context> ctx);
int my_close(int fd);
ssize_t my_pread(int fd, void *buf, size_t count, off_t offset);
ssize_t my_write(int fd, const void *buf, size_t count);
```


In particular `my_open` also needs a `Context` object. This context helps the backend decide the appropriate read/write logic. Below is an example of this `Context` object.

```cpp
bool is_pmem = false;
FileAccessType access_type = SST_Read;

fd = my_open(actualPath.c_str(), flags, mode, 
    std::make_shared<Context>(access_type, is_pmem)
);
```

If other system calls also manipulate the SST or WAL files, their new corresponding versions also need to be used

```cpp
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
```
