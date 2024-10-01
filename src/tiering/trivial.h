#ifndef MY_TRIVIAL_H
#define MY_TRIVIAL_H

#include <mutex>
#include "../global.h"

bool inter_device_copy(copy_info cp_info);

//register file as current
void registerFileAsCurrent(std::string filename);
void unregisterFileAsCurrent(std::string filename);
void printCurrentFiles();
bool isFileCurrent(std::string filename);
void printCurrentOpenedFiles();




#endif  // MY_TRIVIAL_H
