
#include "trivial.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libpmem.h>

#include <iostream>
#include <vector>
#include <set>
#include <regex>
#include <mutex>
#include <memory>
#include <utility>

#include "../global.h"
#include "tiering.h"
#include "yaml-cpp/yaml.h"



std::map<std::string, int> current_files_map;
std::mutex current_files_map_lock;

//register file as current
void registerFileAsCurrent(std::string filename) {
  current_files_map_lock.lock();
  current_files_map[filename] += 1;
  current_files_map_lock.unlock();
}

//unregister file as current
void unregisterFileAsCurrent(std::string filename) {
  current_files_map_lock.lock();
  current_files_map[filename] -= 1;
  // if less than 0, set to 0
  if (current_files_map[filename] < 0) {
    current_files_map[filename] = 0;
  }
  current_files_map_lock.unlock();
}

//print current files
void printCurrentFiles() {
  for( std::map<std::string, int>::iterator i=current_files_map.begin(); i!=current_files_map.end(); ++i ) {
    std::cout << "file: " << i->first << " | count: " << i->second << std::endl;
  }
}

//check if file is current
bool isFileCurrent(std::string filename) {
  current_files_map_lock.lock();
  bool is_current = current_files_map[filename] > 0;
  current_files_map_lock.unlock();
  return is_current;
}

//print current files with count > 0
void printCurrentOpenedFiles() {
  for( std::map<std::string, int>::iterator i=current_files_map.begin(); i!=current_files_map.end(); ++i ) {
    if (i->second > 0) {
      std::cout << "file: " << i->first << " | count: " << i->second << std::endl;
    }
  }
}


#define BUF_LEN 4096


class FileCopyManager {
  public:
    virtual std::pair<bool, std::vector<void*>> openFrom() = 0;
    virtual bool openTo(std::vector<void*> args) = 0;
    virtual ssize_t copyTo(const void* src, size_t to_write) = 0;
    virtual void copyFrom(FileCopyManager* to) = 0;
    virtual int _close() = 0;
};

class FileCopyManagerPosix : public FileCopyManager {
  public:
    char buf[BUF_LEN];
    const char* file;
    int fd;
    int cc;
    struct stat stat;

    FileCopyManagerPosix(const char* file_) : file(file_) {
    }

    ~FileCopyManagerPosix() {
      //TODO: dont assume file is open
      close(fd);
    }

    int _close() {
      return close(fd);
    }

    std::pair<bool, std::vector<void*>> openFrom() {
      fd = open(file, O_RDONLY);

      if (fd < 0) {
        std::cout << "copy manager (posix openFrom): open failed" << std::endl;
        std::cout << "Error opening file " << file << ": " << strerror(errno) << std::endl;
        //print error
        std::cout << "errno: " << errno << std::endl;

        //kill the process
        // abort();


        return std::make_pair(false,std::vector<void*>());
      } else {
          // std::cout << "getting stat for srcfile " << srcfile << std::endl;
        if (fstat(fd, &stat) < 0) {
            close(fd);
            throw std::runtime_error(std::string("fstat failed"));
        }
      }
      std::vector<void*> args = {(void*)stat.st_size};
      // std::pair<bool, std::vector<void*>> p = 
      return std::make_pair(true,args);
    }

    bool openTo(std::vector<void*> args) {
      //TODO: dont assume it works
      fd = open(file, O_RDWR | O_CREAT, 0644);
      if (fd < 0) {
        std::cout << "copy manager (posix openTo): open failed" << std::endl;
        return false;
      }
      return true;  
    }

    ssize_t copyTo(const void* src, size_t to_write) {
      return write(fd, src, to_write);
    }

    void copyFrom(FileCopyManager* to) {
      posix_fadvise(fd, 0, stat.st_size, POSIX_FADV_SEQUENTIAL);

      ssize_t res = readahead(fd, 0, stat.st_size);

      while ((cc = read(fd, buf, BUF_LEN)) > 0) {
        to->copyTo(buf, cc);
      }
      if (cc < 0) {
        perror("read");
        exit(1);
      }

      posix_fadvise(fd, 0, stat.st_size, POSIX_FADV_DONTNEED);
      close(fd);
    }
};


class FileCopyManagerPmdk : public FileCopyManager {
  public:
    size_t mapped_len_;  // the length of the mapped file
    uint8_t* pmemaddr_;  // the base address of data
    int is_pmem;
    const char* file;

    FileCopyManagerPmdk(const char* file_) : file(file_) {
    }

    ~FileCopyManagerPmdk() {
      pmem_drain();
      pmem_unmap(pmemaddr_, mapped_len_);
    }

    int _close() {
      pmem_drain();
      pmem_unmap(pmemaddr_, mapped_len_);
      return 0;
    }

    std::pair<bool, std::vector<void*>> openFrom() {
      pmemaddr_ = (uint8_t*)pmem_map_file(file, 0, PMEM_FILE_EXCL, 0,
                                        &mapped_len_, &is_pmem);
      if (pmemaddr_ == NULL) {
        std::cout << "trivial move: pmem_map_file failed" << std::endl;
        std::cout << "error: " << std::strerror(errno) << std::endl;
        std::cout << "pmem_error6: " << pmem_errormsg() << std::endl;
        return std::make_pair(false,std::vector<void*>());
      }
      return std::make_pair(true,std::vector<void*>());
    }

    bool openTo(std::vector<void*> args) {
      size_t size = (size_t)args[0];

      std::cout << "was told size: " << size << std::endl;

      if ((pmemaddr_ = (uint8_t*)pmem_map_file(file, size,
        PMEM_FILE_CREATE|PMEM_FILE_EXCL,
        0666, &mapped_len_, &is_pmem)) == NULL) {
          perror("pmem_map_file");
          std::cout << "error: " << std::strerror(errno) << std::endl;
          std::cout << "pmem_error7: " << pmem_errormsg() << std::endl;
          return false;
      }
      return true;
    }

    void copyFrom(FileCopyManager* to) {
      int at_each_write = BUF_LEN;

      // std::cout << "copyFrom - copying file: " << file << std::endl;
      // //if file contains the string 146 somewhere print yes!
      // std::regex e(".*146.*");
      // if (std::regex_match(file, e)) {
      //   std::cout << "copyFrom - file contains 146" << std::endl;
      // }
    
      // int x = 0;
      for (size_t i = 0; i < mapped_len_; i += at_each_write) {
        size_t to_write = std::min((size_t)at_each_write, mapped_len_ - i);
        // int x = write(destfd, pmemaddr_ + i, to_write);
        to->copyTo(pmemaddr_ + i, to_write);
        // x++;
      }
      // std::cout << "copyFrom - copied file: " << file << " in " << x << " iterations" << std::endl;
      pmem_unmap(pmemaddr_, mapped_len_);

    }

    ssize_t copyTo(const void* src, size_t to_write) {
      pmem_memcpy_nodrain(pmemaddr_, src, to_write);
      pmemaddr_ += to_write;
    }
};


FileCopyManager* envToManager(EnvEnum env, const char* file) {
  switch (env) {
    case POSIX:
      return new FileCopyManagerPosix(file);
    case PMDK:
      return new FileCopyManagerPmdk(file);
    default:
      throw std::runtime_error(std::string("env not supported"));
  }
}


bool inter_device_copy(copy_info cp_info) {

  std::cout << "started copy of file: " << cp_info.filename << " from " << cp_info.src_path << " to " << cp_info.dest_path << std::endl;

  const char* srcfile = cp_info.src_path.c_str();
  const char* destfile = cp_info.dest_path.c_str();
  EnvEnum envFrom = cp_info.envFrom;
  EnvEnum envTo = cp_info.envTo;

  FileCopyManager* from = envToManager(envFrom, srcfile);
  FileCopyManager* to = envToManager(envTo, destfile);

  std::pair<bool, std::vector<void*>> open_result = from->openFrom();



  if (!open_result.first) {
    return false;
  }
  if (!to->openTo(open_result.second)) {
    return false;
  }
  from->copyFrom(to);


  // std::cout << "ended copy of file: " << cp_info.filename << std::endl;

  //close both
  from->_close();
  to->_close();

  return true;
}

