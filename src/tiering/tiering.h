#ifndef MY_MID_H
#define MY_MID_H

#include <map>
#include <string>

#include "oneapi/tbb/concurrent_hash_map.h"
#include "../../include/myposix.h"
#include "../global.h"





struct compaction_details {
  bool onGoing;
  int new_level;
};

// map of thread ids to compaction details
typedef tbb::concurrent_hash_map<pthread_t, op_type> IdTypeTable;
typedef tbb::concurrent_hash_map<int, std::shared_ptr<Device>> IntDeviceTable;

std::string getFileExtension();
compaction_details* getCompactionDetails(int compaction_id);
void registerThread(pthread_t thread_id, op_type type);
void registerStartCompaction(pthread_t compaction_id, int new_level);
op_type getThreadType(pthread_t thread_id);
void linkFileToDevice(int sst_number, std::shared_ptr<Device> device);
std::shared_ptr<Device> getDevice(int sst_number);
void removeFileDevice(int sst_number);
void printFileDeviceMap();
void dumpFileDeviceMapToFile(std::string filename);
void loadFileDeviceMapFromFile(std::string filename);

bool isThreadACompaction(op_type thread);
int getLevelFromOpType(op_type thread);

bool isSST(std::string filename);
int isSSTorWal(std::string filename);

std::string fileActualPath(std::string fname, std::shared_ptr<Device>& device, int& level);
void read_config_file();
void dumpRedirectionMap();
void read_second_config();
void read_second_config_lrus();
void dump_second_config();


extern std::shared_ptr<Device> wal_device;


#endif  // MY_MID_H