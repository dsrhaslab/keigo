#ifndef MY_THREAD_TEST
#define MY_THREAD_TEST

#include <string>

#include "../../include/myposix.h"
#include "../global.h"

//TODO remove the first one
void add_file_to_trivialmove_queue(std::string file, copy_type type);
void add_file_to_trivialmove_queue(copy_info cp_info);
void start_thread();
void stop_trivial_move();


#endif  // MY_THREAD_TEST
