#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <queue>



#include "thread_test.h"
#include "tiering.h"
#include "trivial.h"
#include "trivial.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../global.h"


#include "../../include/keigo.h"



//queue of trivially moved files (pair of string and copy_type)
std::queue<copy_info>* trivial_queue;

std::thread lru_thread;


std::mutex queue_lock;
std::condition_variable new_file_submitted;



void add_file_to_trivialmove_queue(std::string file, copy_type type) {
  
}

void add_file_to_trivialmove_queue(copy_info cp_info) {
  std::lock_guard<std::mutex> guard(queue_lock);
  trivial_queue->push(cp_info);
  //print length of queue

  new_file_submitted.notify_all();
}

void stop_trivial_move() {
  {
  std::lock_guard<std::mutex> guard(queue_lock);
  trivial_move_active = false;
  new_file_submitted.notify_all();
  }
  lru_thread.join();
}


void* queue_worker() {


  while (trivial_move_active) {
    std::unique_lock<std::mutex> lock(queue_lock);

    while (trivial_queue->empty() && trivial_move_active) {
      new_file_submitted.wait(lock);
    }

    if (!trivial_queue->empty()) {

      

      copy_info cp_info = trivial_queue->front();
      trivial_queue->pop();

      //unlock mutex
      lock.unlock();

      std::cout << "trivial move: " << cp_info.src_path << " to " << cp_info.dest_path  << "level: "  << cp_info.dest_level << std::endl;

      bool copied = inter_device_copy(cp_info);

      if (copied) {
        add_sst_level(cp_info.sst_number,cp_info.dest_level);
        
        #ifdef PROFILER_3000
          std::ostringstream oss;
          oss << "m " << cp_info.sst_number << " " << cp_info.dest_tier_name << " l" << cp_info.dest_level << std::endl;
          writePROFILING(oss.str());
        #endif  // PROFILER_3000

        FileAccessType access_type = SST_Read;
        bool is_pmem = false; //TODO... DEPENDE

        reset_file_env((cp_info.dest_path),std::make_shared<Context>(access_type, is_pmem), name_to_device_map[cp_info.dest_tier_name], false);
        
        
                            
        std::shared_ptr<Device> device = name_to_device_map[cp_info.dest_tier_name];


        if (device->lru_working) {
            device->lru->add(cp_info.sst_number);
        }

        int u = unlink(cp_info.src_path.c_str());
        //show error if unlink fails (perror)
        if (u != 0)
          std::cout << "Error unlinking file " << ": " << strerror(errno) << std::endl;
        

      }   

    } 
    
    if(trivial_queue->empty() && trivial_move_active == false) {
      return NULL;
    }
  }

}



//start the thread
void start_thread() {
  //initialize the queue
  trivial_queue = new std::queue<copy_info>;

  lru_thread = std::thread(queue_worker);

}


