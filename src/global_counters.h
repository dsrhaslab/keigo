#ifndef COUNTERS_H
#define COUNTERS_H

#include <mutex>
#include <iostream>

// have a counter for each function

int walread_constructor_counter = 0;
int walread_destructor_counter = 0;
int walread_read_counter = 0;
int walread_skip_counter = 0;

int walwrite_constructor_counter = 0;
int walwrite_close_counter = 0;
int walwrite_append_counter = 0;
int walwrite_truncate_counter = 0;

int sstread_constructor_counter = 0;
int sstread_destructor_counter = 0;
int sstread_read_counter = 0;

int sstwrite_constructor_counter = 0;
int sstwrite_close_counter = 0;
int sstwrite_mapnewregion_counter = 0;
int sstwrite_append_counter = 0;
int sstwrite_truncate_counter = 0;
int sstwrite_sync_counter = 0;
int sstwrite_fsync_counter = 0;



// have a mutex for each function
std::mutex walread_constructor_mutex;
std::mutex walread_destructor_mutex;
std::mutex walread_read_mutex;
std::mutex walread_skip_mutex;

std::mutex walwrite_constructor_mutex;
std::mutex walwrite_close_mutex;
std::mutex walwrite_append_mutex;
std::mutex walwrite_truncate_mutex;

std::mutex sstread_constructor_mutex;
std::mutex sstread_destructor_mutex;
std::mutex sstread_read_mutex;

std::mutex sstwrite_constructor_mutex;
std::mutex sstwrite_close_mutex;
std::mutex sstwrite_mapnewregion_mutex;
std::mutex sstwrite_append_mutex;
std::mutex sstwrite_truncate_mutex;
std::mutex sstwrite_sync_mutex;
std::mutex sstwrite_fsync_mutex;


// have an add method for each function
void add_walread_constructor_counter() {
    std::lock_guard<std::mutex> lock(walread_constructor_mutex);
    walread_constructor_counter++;
}

void add_walread_destructor_counter() {
    std::lock_guard<std::mutex> lock(walread_destructor_mutex);
    walread_destructor_counter++;
}

void add_walread_read_counter() {
    std::lock_guard<std::mutex> lock(walread_read_mutex);
    walread_read_counter++;
}

void add_walread_skip_counter() {
    std::lock_guard<std::mutex> lock(walread_skip_mutex);
    walread_skip_counter++;
}



void add_walwrite_constructor_counter() {
    std::lock_guard<std::mutex> lock(walwrite_constructor_mutex);
    walwrite_constructor_counter++;
}

void add_walwrite_close_counter() {
    std::lock_guard<std::mutex> lock(walwrite_close_mutex);
    walwrite_close_counter++;
}

void add_walwrite_append_counter() {
    std::lock_guard<std::mutex> lock(walwrite_append_mutex);
    walwrite_append_counter++;
}

void add_walwrite_truncate_counter() {
    std::lock_guard<std::mutex> lock(walwrite_truncate_mutex);
    walwrite_truncate_counter++;
}




void add_sstread_constructor_counter() {
    std::lock_guard<std::mutex> lock(sstread_constructor_mutex);
    sstread_constructor_counter++;
}

void add_sstread_destructor_counter() {
    std::lock_guard<std::mutex> lock(sstread_destructor_mutex);
    sstread_destructor_counter++;
}

void add_sstread_read_counter() {
    std::lock_guard<std::mutex> lock(sstread_read_mutex);
    sstread_read_counter++;
}




void add_sstwrite_constructor_counter() {
    std::lock_guard<std::mutex> lock(sstwrite_constructor_mutex);
    sstwrite_constructor_counter++;
}

void add_sstwrite_close_counter() {
    std::lock_guard<std::mutex> lock(sstwrite_close_mutex);
    sstwrite_close_counter++;
}

void add_sstwrite_mapnewregion_counter() {
    std::lock_guard<std::mutex> lock(sstwrite_mapnewregion_mutex);
    sstwrite_mapnewregion_counter++;
}

void add_sstwrite_append_counter() {
    std::lock_guard<std::mutex> lock(sstwrite_append_mutex);
    sstwrite_append_counter++;
}

void add_sstwrite_truncate_counter() {
    std::lock_guard<std::mutex> lock(sstwrite_truncate_mutex);
    sstwrite_truncate_counter++;
}

void add_sstwrite_sync_counter() {
    std::lock_guard<std::mutex> lock(sstwrite_sync_mutex);
    sstwrite_sync_counter++;
}

void add_sstwrite_fsync_counter() {
    std::lock_guard<std::mutex> lock(sstwrite_fsync_mutex);
    sstwrite_fsync_counter++;
}

// print the counters
void print_counters() {
    std::cout << "walread_constructor_counter: " << walread_constructor_counter << std::endl;
    std::cout << "walread_destructor_counter: " << walread_destructor_counter << std::endl;
    std::cout << "walread_read_counter: " << walread_read_counter << std::endl;
    std::cout << "walread_skip_counter: " << walread_skip_counter << std::endl;

    std::cout << "walwrite_constructor_counter: " << walwrite_constructor_counter << std::endl;
    std::cout << "walwrite_close_counter: " << walwrite_close_counter << std::endl;
    std::cout << "walwrite_append_counter: " << walwrite_append_counter << std::endl;
    std::cout << "walwrite_truncate_counter: " << walwrite_truncate_counter << std::endl;

    std::cout << "sstread_constructor_counter: " << sstread_constructor_counter << std::endl;
    std::cout << "sstread_destructor_counter: " << sstread_destructor_counter << std::endl;
    std::cout << "sstread_read_counter: " << sstread_read_counter << std::endl;

    std::cout << "sstwrite_constructor_counter: " << sstwrite_constructor_counter << std::endl;
    std::cout << "sstwrite_close_counter: " << sstwrite_close_counter << std::endl;
    std::cout << "sstwrite_mapnewregion_counter: " << sstwrite_mapnewregion_counter << std::endl;
    std::cout << "sstwrite_append_counter: " << sstwrite_append_counter << std::endl;
    std::cout << "sstwrite_truncate_counter: " << sstwrite_truncate_counter << std::endl;
    std::cout << "sstwrite_sync_counter: " << sstwrite_sync_counter << std::endl;
    std::cout << "sstwrite_fsync_counter: " << sstwrite_fsync_counter << std::endl;
}

#endif  // COUNTERS_H
