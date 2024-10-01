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


#include "../../include/myposix.h"



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
  // std::cout << "added file to trivial queue: " << cp_info.src_path << std::endl;

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

      // struct stat stat_buf_src;
      // struct stat stat_buf_dest;

      // if (stat(cp_info.src_path.c_str(), &stat_buf_src) == 0) {
      // } else {
      //     std::cerr << "Error: Could not get file statistics.\n";
      
      // }

      // if (stat(cp_info.dest_path.c_str(), &stat_buf_dest) == 0) {
      // } else {
      //     std::cerr << "Error: Could not get file statistics.\n";
      // }

      // std::cout << "trivial move: " << cp_info.src_path << " to " << cp_info.dest_path  << "level: "  << cp_info.dest_level << " size: " << stat_buf_src.st_size << " -> " << stat_buf_dest.st_size << std::endl;

        add_sst_level(cp_info.sst_number,cp_info.dest_level);
        // std::cout << "trivial move copied file: " << cp_info.src_path << " to " << cp_info.dest_path  << "level: "  << cp_info.dest_level << std::endl;

        #ifdef PROFILER_3000
          std::ostringstream oss;
          oss << "m " << cp_info.sst_number << " " << cp_info.dest_tier_name << " l" << cp_info.dest_level << std::endl;
          writePROFILING(oss.str());
        #endif  // PROFILER_3000

        FileAccessType access_type = SST_Read;
        bool is_pmem = false; //TODO... DEPENDE

        reset_file_env((cp_info.dest_path),std::make_shared<Context>(access_type, is_pmem), name_to_device_map[cp_info.dest_tier_name], false);
        
        
                            
        std::shared_ptr<Device> device = name_to_device_map[cp_info.dest_tier_name];

        // std::shared_ptr<Device> device = name_to_device_map[2];


        if (device->lru_working) {
            // std::cout << "adding to lru after trivial" << std::endl;


                    // struct stat stat_buf;
    
                    // if (stat(cp_info.dest_path.c_str(), &stat_buf) == 0) {
                    //     std::cout << "File4 size of " << cp_info.dest_path.c_str() << " is " << stat_buf.st_size << " bytes.\n";
                    // } else {
                    //     std::cerr << "Error: Could not get file statistics.\n";

                    // }

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


//Thread function
// void* queue_worker(void *ptr) {
    

//     while (trivial_move_active) {
//         // Lock mutex and then wait for signal to relase mutex
//         pthread_mutex_lock(&thread_lock);
//         while (trivial_queue->empty() && trivial_move_active) {

//             pthread_cond_wait(&new_file_cond1, &thread_lock);
            
//         }

//         if (!trivial_queue->empty()) {
//         } else if(trivial_move_active == false) {
//             pthread_mutex_unlock(&thread_lock);
//             return NULL;
//         }
//             // Unlock mutex
//         pthread_mutex_unlock(&thread_lock);

//         // Get the first element from queue

//         pthread_mutex_lock(&trivial_queue_lock);
//         copy_info cp_info = trivial_queue->front();
//         trivial_queue->pop();
//         pthread_mutex_unlock(&trivial_queue_lock);


    


//         // print the first element from the file pair

//         bool copied = inter_device_copy(cp_info);


//         if (copied) {
//           add_sst_level(cp_info.sst_number,cp_info.dest_level);

//           std::cout << "trivial move (or lru) copied file: " << cp_info.src_path << " to " << cp_info.dest_path  << "level: "  << cp_info.dest_level << std::endl;

//           #ifdef PROFILER_3000
//             std::ostringstream oss;
//             oss << "m " << cp_info.sst_number << " " << cp_info.dest_tier_name << " l" << cp_info.dest_level << std::endl;
//             writePROFILING(oss.str());
//           #endif  // PROFILER_3000

//         //   std::cout << "t5.1" << std::endl;


//           FileAccessType access_type = SST_Read;
//           bool is_pmem = false; //TODO... DEPENDE
//           reset_file_env((cp_info.dest_path),std::make_shared<Context>(access_type, is_pmem), name_to_device_map[cp_info.dest_tier_name], false);


//         // std::cout << "t5.2" << std::endl;

//           unlink(cp_info.src_path.c_str());
          
//         }   


//         // std::cout << "t6" << std::endl;
//     }

// }


//start the thread
void start_thread() {
  //initialize the queue
  trivial_queue = new std::queue<copy_info>;

  lru_thread = std::thread(queue_worker);

  // pthread_t thread1;
  // pthread_create(&thread1, NULL, &queue_worker, NULL);

  // pthread_t thread2;
  // pthread_create(&thread2, NULL, &queue_worker, NULL);

  // pthread_t thread3;
  // pthread_create(&thread3, NULL, &queue_worker, NULL);

  // pthread_t thread4;
  // pthread_create(&thread4, NULL, &queue_worker, NULL);

  // std::thread* pThread1 = nullptr;
  // if (FLAGS_benchmarks.find("ycsbwkldxyz") != std::string::npos) {
  //         pThread1 = new std::thread(caching_thread);
  //         //detach the thread and continue with the main thread
  //         pThread1->detach();
  // }

  // pthread_t thread2;
  // pthread_create(&thread2, NULL, &caching_thread, NULL);


}




//Thread function
// void* queue_worker2(void *ptr) {
    
//     while (trivial_move_active) {
//         std::cout << "t1" << std::endl;
//         // Lock mutex and then wait for signal to relase mutex
//         pthread_mutex_lock(&queue_lock1);
//         while (trivial_queue->empty() && trivial_move_active) {
//             std::cout << "t1.1" << std::endl;

//             pthread_cond_wait(&new_file_cond1, &queue_lock1);
//             if (trivial_move_active == false) {
//               pthread_mutex_unlock(&queue_lock1);
//               return NULL;
//             }
//         }

//         std::cout << "t2" << std::endl;

//         // Get the first element from queue
//         copy_info cp_info = trivial_queue->front();
//         trivial_queue->pop();

//         std::cout << "t3" << std::endl;


//         // Unlock mutex
//         pthread_mutex_unlock(&queue_lock1);
//         // print the first element from the file pair

//         std::cout << "t4" << std::endl;



//         bool copied = inter_device_copy(cp_info);

//         std::cout << "t5" << std::endl;

//         if (copied) {
//           add_sst_level(cp_info.sst_number,cp_info.dest_level);

//           std::cout << "trivial move (or lru) copied file: " << cp_info.src_path << " to " << cp_info.dest_path  << "level: "  << cp_info.dest_level << std::endl;

//           #ifdef PROFILER_3000
//             std::ostringstream oss;
//             oss << "m " << cp_info.sst_number << " " << cp_info.dest_tier_name << " l" << cp_info.dest_level << std::endl;
//             writePROFILING(oss.str());
//           #endif  // PROFILER_3000

//           std::cout << "t5.1" << std::endl;


//           FileAccessType access_type = SST_Read;
//           bool is_pmem = false; //TODO... DEPENDE
//           reset_file_env((cp_info.dest_path),std::make_shared<Context>(access_type, is_pmem), name_to_device_map[cp_info.dest_tier_name], false);


//         std::cout << "t5.2" << std::endl;

//           unlink(cp_info.src_path.c_str());
          
//         }   

//         std::cout << "t6" << std::endl;

//     }
// }