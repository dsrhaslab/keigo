#include "../include/keigo.h"

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
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>

#include "spinlock.h"

#include "file_envs/file_env.h"
#include "file_envs/posix_file_env.h"
#include "file_envs/pmem_file_env.h"
#include "file_envs/wal_file_env.h"

#include "file_env_crud.h"
#include "global.h"


#include "tiering/tiering.h"
#include "tiering/trivial.h"
#include "tiering/thread_test.h"
#include "spinlock.h"


#include "caching_system.h"



void init_tiering_lib() {

    //start a timer with chrono
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    init_free_fds();
    read_config_file();
    read_second_config();
    read_second_config_lrus();

    #ifdef PROFILER_3000
        std::cout << "profiler is on" << std::endl;
        openPROFILINGfile();
        // openPROFILINGfile2();

    #endif  // PROFILER_3000

    start_thread();

    //end timer
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Time to init tiering lib = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
}

void end_tiering_lib() {
    //TODO juntar isto tudo numa só
    #ifdef PROFILER_3000
        stopPROFILING();
        // stopPROFILING2();

    #endif  // PROFILER_3000
    trivial_move_active = false;

    stop_trivial_move();

    stopLRUs();

    for (std::shared_ptr<DeviceCache> device : caches) {

        device.get()->stopThreads();


    }

    // pthread_mutex_lock(&queue_under_work_lock);
    dumpRedirectionMap();
    dump_second_config();
    print_sst_file_number_to_string_list_map();
}



//map that holds the fake fd for each file
std::map<std::string, int> fake_fd_map;
// shared timed mutex for fake_fd_map
std::shared_timed_mutex fake_fd_map_mutex;

//function to get the fake fd for a file
int get_fake_fd(std::string file_path) {
    std::shared_lock<std::shared_timed_mutex> lock(fake_fd_map_mutex);
    if (fake_fd_map.find(file_path) != fake_fd_map.end()) {
        return fake_fd_map[file_path];
    }
    return -1;
}

//function to add a fake fd for a file
void add_fake_fd(std::string file_path, int fake_fd) {
    std::unique_lock<std::shared_timed_mutex> lock(fake_fd_map_mutex);
    fake_fd_map[file_path] = fake_fd;
}

//function to remove a fake fd for a file
void remove_fake_fd(std::string file_path) {
    std::unique_lock<std::shared_timed_mutex> lock(fake_fd_map_mutex);
    fake_fd_map.erase(file_path);
}



void enqueueTrivialMove(std::string filename, int input_level, int output_level) {


    std::string filename_ = filename.substr(0, filename.find_last_of("."));
    int sst_number = std::stoi(filename_);

    std::shared_ptr<Device> input_device = getDevice(sst_number);

    std::shared_ptr<Device> output_device = getDeviceFromLevel(output_level);

    if (input_device.get() != output_device.get()) {

        std::string input_file = input_device.get()->path + "/" + filename;
        std::string output_file = output_device.get()->path + "/" + filename;

        copy_info cp_info;
        cp_info.filename = filename;
        cp_info.src_path = input_file;
        cp_info.dest_path = output_file;
        cp_info.envFrom = input_device.get()->env;
        cp_info.envTo = output_device.get()->env;
        cp_info.dest_tier_name = output_device->name;
        cp_info.dest_level = output_level;
        //remove extension from filename
        std::string filename_ = filename.substr(0, filename.find_last_of("."));
        cp_info.sst_number = std::stoi(filename_);

        add_file_to_trivialmove_queue(cp_info);

    } else {

        if (input_device->lru) {
            input_device->lru->changeLevel(sst_number, input_level, output_level);   
        }
        add_sst_level(sst_number,output_level);
        

        #ifdef PROFILER_3000
            std::ostringstream oss;
            oss << "l " << sst_number << " l" << output_level << std::endl;
            writePROFILING(oss.str());
        #endif  // PROFILER_3000

        //no need to move
    }
    
}

bool getFileActualPathOpen(std::string fname, std::string &actualPath_ret, std::shared_ptr<Context>& context) {
    int fileType = isSSTorWal(fname);
    if (fileType != 0) {
        bool is_pmem = false;
        // FileAccessType access_type;
        int level = -1;

        std::shared_ptr<Device> device = nullptr;
        std::string actualPath = fileActualPath(fname, device, level);


        std::string sst_number_str = fname.substr(fname.length() - 10);
        sst_number_str = sst_number_str.substr(0, sst_number_str.length() - 4);
        int sst_number = std::stoi(sst_number_str);
        add_sst_level(sst_number, level);

        if (fileType == 1)
            linkFileToDevice(sst_number, device);

        if (device && device->env != EnvEnum::POSIX) {
          is_pmem = true;
        }

        actualPath_ret = actualPath;
        context = std::make_shared<Context>(ACCESSOR_OTHER, is_pmem);
        return true;

    }
    return false;
}


std::string getFileActualPath(std::string fname) {
    std::shared_ptr<Device> device = nullptr;
    int level = -1;

    return fileActualPath(fname, device, level);
}


std::string getFileActualPathUnlink(std::string fname, std::shared_ptr<Context>& context) {
    std::shared_ptr<Device> device = nullptr;
    int level = -1;

    std::string actualPath = fileActualPath(fname, device, level);

    bool is_pmem = false;
    Context* ctx;

    if (device && device->env != EnvEnum::POSIX) {
      is_pmem = true;
    }

    context = std::make_shared<Context>(ACCESSOR_OTHER, is_pmem);
    return actualPath;
}


// using namespace keigo;

#define RECYCLE_SUFFIX ".recycle"


const size_t max_recycled_sst_files_ = 100;
const size_t max_recycled_wal_files_ = 100;

std::vector<std::string> recycled_sst_;
MySpinMutex recycle_sst_lock_;

std::vector<std::string> recycled_wal_;
MySpinMutex recycle_wal_lock_;

//function that renames a file using normal posix rename
int renamefile(std::string filename_before, std::string filename_after) {
    int ret = rename(filename_before.c_str(), filename_after.c_str());
    if (ret != 0) {
        std::cout << "rename failed: " << filename_before << " to " << filename_after << std::endl;
    }
    return ret;
}


bool recycle_wal_file(std::string pathname, std::string recycled_fname) {
    bool recycled = false;
    std::unique_lock<MySpinMutex> lock(recycle_wal_lock_);
    if (recycled_wal_.size() < max_recycled_wal_files_ &&
        renamefile(pathname, recycled_fname) == 0) {
        recycled_wal_.push_back(recycled_fname);
        recycled = true;
    }
    return recycled;
}

bool recycle_sst_file(std::string pathname, std::string recycled_fname) {
    bool recycled = false;
    std::unique_lock<MySpinMutex> lock(recycle_sst_lock_);
    if (recycled_sst_.size() < max_recycled_sst_files_ &&
        renamefile(pathname, recycled_fname) == 0) {
        recycled_sst_.push_back(recycled_fname);
        recycled = true;
    }
    return recycled;
}

int unlink_logic(std::string pathname) {
     //get filename from filepath (fname_)
    std::string filename_orig = pathname.substr(pathname.find_last_of("/\\") + 1);
    //remove file extension
    std::string filename = filename_orig.substr(0, filename_orig.find_last_of(".")); 
    // //get number from file name (remove .sst)
    int sst_number = std::stoi(filename);


    // if (is_file_in_files_in_cache(sst_number)) {
    //     remove_file_from_files_in_cache(sst_number);
    //     std::string core = getGlobalSSDPath() + "/" + filename_orig;
    //     std::cout << "file is cached, going to also unlink core: " << core << std::endl;
    //     unlink(core.c_str());
    // }

    //extract path without filename and without the last "/"
    std::string path = pathname.substr(0, pathname.find_last_of("/\\"));
        
    return 1;

}


int k_unlink(const char *pathname, std::shared_ptr<Context> ctx) {
    std::cout << "k_unlink: " << pathname << std::endl;
    int ret = 0;
    if (ctx->is_pmem_) {

        //get last 4 chars of pathname
        std::string path(pathname); 
        std::string ext = path.substr(path.length() - 4);

        bool recycled = false;

        if (ext == ".log") {
            std::string recycled_fname(pathname);
            recycled_fname.append(RECYCLE_SUFFIX);

            recycled = recycle_wal_file(pathname, recycled_fname);
        } else if (ext == getFileExtension()) {

            std::string recycled_fname(pathname);
            recycled_fname.append(RECYCLE_SUFFIX);

            recycled = recycle_sst_file(pathname, recycled_fname);
        } else {

            ret = unlink(pathname);
        }

        if (recycled) {
            ret = 0;
        } else {
            ret = unlink(pathname);

        }
        unlink_logic(pathname);
    } else {
        ret = unlink(pathname);

    }

    //get filename from filepath (fname_)
    std::string pathname_(pathname);
    std::string ext = pathname_.substr(pathname_.length() - 4);
    std::string filename = pathname_.substr(pathname_.find_last_of("/\\") + 1);

    try
    {
        if (ext == getFileExtension()) {
            int sst_number = std::stoi(filename);

            #ifdef PROFILER_3000
                std::ostringstream oss;
                oss << "u " << sst_number << std::endl;
                writePROFILING(oss.str());
            #endif  // PROFILER_3000

            std::shared_ptr<Device> device = getDevice(sst_number);


            if (device->lru) {
                device->lru->remove(sst_number);
                std::cout << "removed from lru: " << sst_number << std::endl;

                if (device->cache) {
                    device->cache->removeSstTotal(sst_number);
                    
                }
            }

            if (device->next != nullptr) {

                std::string other = device->next->path + "/" + filename;

                unlink(other.c_str());
            }

            if (device->cache != nullptr) {
                device->cache->removeSstTotal(sst_number);
            }
        }
    }
    catch(const std::exception& e)
    {
    }
    

    return ret;
}


int open_logic(int real_fd, const char *pathname, int flags, bool has_mode, 
                mode_t mode, std::shared_ptr<Context> ctx) {
    uint32_t fake_fd;

    if (!(ctx->is_pmem_)) { 
        fake_fd = CreateFileEnvPosix(real_fd, pathname);
    } else {
        if (ctx->type_ == SST_Read ) {
            fake_fd = CreateFileEnvPmem(real_fd, pathname);
        }
        else if (ctx->type_ == SST_Write) {
            fake_fd = CreateSstWriteFileEnv(real_fd, pathname);
   
        } else if (ctx->type_ == WAL_Read) {
            fake_fd = CreateWalReadFileEnv(real_fd, pathname);
        } else if (ctx->type_ == WAL_Write) {
            fake_fd = CreateWalWriteFileEnv(real_fd, pathname);
        } else {
            fake_fd = CreateFileEnvPosix(real_fd, pathname);
        }
    }


    std::cout << "open_logic: " << pathname << " , real fd:" << real_fd << " , fake fd:" << fake_fd << std::endl;

    //get filename and extension, separately
    std::string path(pathname);
    std::string filename = path.substr(path.find_last_of("/\\") + 1);
    std::string ext = path.substr(path.length() - 4);
    //remove extension from filename
    filename = filename.substr(0, filename.length() - 4);
    if (ext == getFileExtension()) {
        add_sst_file(filename, fake_fd);
    }

    // add_fake_fd(std::string(pathname),fake_fd);
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fake_fd]);
    auto file_env = GetFileEnv(fake_fd);
    if (has_mode) {
        file_env->_open(pathname, flags, mode);
    } else {
        file_env->_open(pathname, flags);
    }
    return fake_fd;
}

bool get_recycled_wal_file(std::string& recycled_fname) {
    bool recycle = false;
    std::unique_lock<MySpinMutex> lock(recycle_wal_lock_);
      if (!recycled_wal_.empty()) {
        recycled_fname = recycled_wal_.back();
        recycled_wal_.pop_back();
        recycle = true;
      }
    return recycle;
}

bool get_recycled_sst_file(std::string& recycled_fname) {
    bool recycle = false;
    std::unique_lock<MySpinMutex> lock(recycle_sst_lock_);
    if (!recycled_sst_.empty()) {
        recycled_fname = recycled_sst_.back();
        recycled_sst_.pop_back();
        recycle = true;
    }
    return recycle;
}


bool new_writable_logic(std::string pathname) {
    //get last 4 chars of pathname
    std::string path(pathname); 
    std::string ext = path.substr(path.length() - 4);

    std::string recycled_fname(pathname);
    bool recycle = false;


    if (ext == ".log") {

        recycle = get_recycled_wal_file(recycled_fname);
    } else if (ext == getFileExtension()) {

        recycle = get_recycled_sst_file(recycled_fname);
    }
    if (recycle) {
        renamefile(recycled_fname, pathname);
        return true;
    }
    return false;
}


int k_open(const char *pathname, int flags, std::shared_ptr<Context> ctx) {

    int fd;

    if (ctx->is_pmem_ && (ctx->type_ == SST_Write || ctx->type_ == WAL_Write)) {
        bool recycle = new_writable_logic(pathname);

        if (ctx->type_ == WAL_Write) {
            if (recycle) {
                fd = open(pathname, (O_RDWR), 0644);
                if (fd == -1) {
                    std::cout << "real-open-error1: " << std::strerror(errno) << std::endl;
                }
               
            }
            else {
                fd = open(pathname, (O_CREAT | O_EXCL | O_RDWR), 0644);
                if (fd == -1) {
                    std::cout << "real-open-error2: " << std::strerror(errno) << std::endl;
                }
            }
        } else { //SST_Write
            fd = 0;
        }
    } else { //not pmem writable
        fd = open(pathname, flags);
                if (fd == -1) {
                    std::cout << "real-open-error3: " << std::strerror(errno) << std::endl;
                }
    }

    return open_logic(fd, pathname, flags, false, 0, ctx);
}

int k_open(const char *pathname, int flags, mode_t mode, std::shared_ptr<Context> ctx) {

    // int fd = open(pathname, flags);
    int fd;
    if (ctx->is_pmem_ && (ctx->type_ == SST_Write || ctx->type_ == WAL_Write)) {
        bool recycle = new_writable_logic(pathname);

        if (ctx->type_ == WAL_Write) {
            if (recycle) {
                fd = open(pathname, (O_RDWR), 0644);
                if (fd == -1) {
                    std::cout << "real-open-error4: " << std::strerror(errno) << std::endl;
                }
            } else {
                fd = open(pathname, (O_CREAT | O_EXCL | O_RDWR), 0644);
                if (fd == -1) {
                    std::cout << "real-open-error5: " << std::strerror(errno) << std::endl;
                    fd = open(pathname, (O_EXCL | O_RDWR), 0644);
                }
            }
        } else { //SST_Write
            fd = 0;
        }
    } else { //not writable
        fd = open(pathname, flags, mode);
                if (fd == -1) {
                    std::cout << "real-open-error6: " << pathname << std::strerror(errno) << std::endl;
                }
    }
    return open_logic(fd, pathname, flags, true, mode, ctx);
}

int k_close(int fd) {
    std::cout << "k_close: fd: " << fd << std::endl;
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return close(fd);
    }
    int ret = file_env->_close(fd, false);

    RemoveFileEnv(fd);
    return ret;
}

ssize_t k_pread(int fd, void *buf, size_t count, off_t offset) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);
    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return pread(fd, buf, count, offset);
    }
    return file_env->_pread(fd, buf, count, offset);
}

void *k_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return mmap(addr, length, prot, flags, fd, offset);
    }
    return file_env->_mmap(addr, length, prot, flags, fd, offset);
}


off_t k_lseek(int fd, off_t offset, int whence) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return lseek(fd, offset, whence);
    }
    return file_env->_lseek(fd, offset, whence);
}

ssize_t k_read(int fd, void *buf, size_t count) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return read(fd, buf, count);
    }
    return file_env->_read(fd, buf, count);
}


ssize_t k_write(int fd, const void *buf, size_t count) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return write(fd, buf, count);
    }
    return file_env->_write(fd, buf, count);
}

int k_fsync(int fd) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return fsync(fd);
    }
    return file_env->_fsync(fd);
}

int k_fdatasync(int fd) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return fdatasync(fd);
    }
    return file_env->_fdatasync(fd);
}

ssize_t k_readahead(int fd, off64_t offset, size_t count) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return readahead(fd, offset, count);
    }
    return file_env->_readahead(fd, offset, count);
}

int k_fcntl(int fd, int cmd, ... /* arg */) {
    // call the original fcntl function with the same arguments
    va_list args;
    va_start(args, cmd);
    void *arg = va_arg(args, void *);
    va_end(args);
    int ret = fcntl(fd, cmd, arg);

    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return fcntl(fd, cmd, arg);
    }
    return file_env->_fcntl(fd, cmd, arg);
}

int k_posix_fadvise(int fd, off_t offset, off_t len, int advice) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return posix_fadvise(fd, offset, len, advice);
    }
    return file_env->_posix_fadvise(fd, offset, len, advice);
}

ssize_t k_pwrite(int fd, const void *buf, size_t count, off_t offset) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return pwrite(fd, buf, count, offset);
    }
    return file_env->_pwrite(fd, buf, count, offset);
}

int k_ftruncate(int fd, off_t length) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return ftruncate(fd, length);
    }
    return file_env->_ftruncate(fd, length);
}

int k_fallocate(int fd, int mode, off_t offset, off_t len) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return fallocate(fd, mode, offset, len);
    }
    return file_env->_fallocate(fd, mode, offset, len);
}

int k_sync_file_range(int fd, off64_t offset, off64_t nbytes, unsigned int flags) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return sync_file_range(fd, offset, nbytes, flags);
    }
    return file_env->_sync_file_range(fd, offset, nbytes, flags);
}

int k_fstat(int fd, struct stat *buf) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fd]);

    auto file_env = GetFileEnv(fd);
    if (file_env == nullptr) {
        return fstat(fd, buf);
    }
    return file_env->_fstat(fd, buf);
}


size_t k_fread_unlocked(void *ptr, size_t size, size_t n, FILE *stream) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fileno(stream)]);

    auto file_env = GetFileEnv(fileno(stream));
    if (file_env == nullptr) {
        return fread_unlocked(ptr, size, n, stream);
    }
    return file_env->_fread_unlocked(ptr, size, n, stream);
}
int k_fseek(FILE *stream, long offset, int whence) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fileno(stream)]);

    auto file_env = GetFileEnv(fileno(stream));
    if (file_env == nullptr) {
        return fseek(stream, offset, whence);
    }
    return file_env->_fseek(stream, offset, whence);
}
int k_fclose(FILE *stream) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fileno(stream)]);

    auto file_env = GetFileEnv(fileno(stream));
    if (file_env == nullptr) {
        return fclose(stream);
    }
    return file_env->_fclose(stream);
}

int k_feof(FILE *stream) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fileno(stream)]);

    auto file_env = GetFileEnv(fileno(stream));
    if (file_env == nullptr) {
        return feof(stream);
    }
    return file_env->_feof(stream);
}

void k_clearerr(FILE *stream) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fileno(stream)]);

    auto file_env = GetFileEnv(fileno(stream));
    if (file_env == nullptr) {
        return clearerr(stream);
    }
    return file_env->_clearerr(stream);
}

FILE *k_fdopen(int fildes, const char *mode) {
    std::shared_lock<std::shared_timed_mutex> lock(*files_locks[fildes]);

    auto file_env = GetFileEnv(fildes);
    if (file_env == nullptr) {
        return fdopen(fildes, mode);
    }
    return file_env->_fdopen(fildes, mode);
}



void reset_file_env(std::string filepath, std::shared_ptr<Context> ctx, std::shared_ptr<Device> device, bool is_caching) {

    std::cout << "reset_file_env: " << filepath << std::endl;

    //get filename from filepath (fname_)
    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);
    //remove file extension
    filename = filename.substr(0, filename.find_last_of(".")); 

    //get file descriptor in map
    int fake_fd = get_fd_from_sst_file(filename);

    if (fake_fd == 0) {
        std::cout << "reset_file_env for " << filepath << ": fake_fd not found" << std::endl;

        // //get number from file name (remove .sst)
        int sst_number = std::stoi(filename);
        linkFileToDevice(sst_number, device);

        return;
    }

    add_sst_file(filename, fake_fd);


    //i know it's never a write
    int fd = open(filepath.c_str(), O_RDONLY, 0644);

    ResetFileEnv(fake_fd, fd, filepath, ctx, device, is_caching);

}