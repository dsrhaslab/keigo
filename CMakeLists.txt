
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-g")
# set(CMAKE_CXX_FLAGS_DEBUG "-O3 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O0")



add_library(myposix SHARED include/myposix.h src/myposix.cc 
src/file_envs/file_env.h
src/file_envs/posix_file_env.h
src/file_envs/sst_write_file_env.h
src/file_envs/pmem_file_env.h
src/file_env_crud.h
src/spinlock.h
src/global_counters.h
src/global.h src/global.cc
src/cache_node.h src/cache_node.cc
src/caching_system.h src/caching_system.cc
src/device.h src/device.cc
src/structs.h src/structs.cc

src/tiering/thread_test.cc src/tiering/thread_test.h
src/tiering/tiering.cc src/tiering/tiering.h
src/tiering/trivial.cc src/tiering/trivial.h
src/lru.cc src/lru.h


)

option(USE_PROFILER_3000 "Generate Log file for PROFILER 3000" OFF)
if (USE_PROFILER_3000)
  add_definitions(-DPROFILER_3000)
endif()

# target_compile_definitions(myposix PRIVATE PROFILER_3000)

include_directories("${CMAKE_SOURCE_DIR}/yaml-cpp/include/")

find_library(YAML_LIBRARY
    NAMES libyaml-cpp.a
    PATHS "${CMAKE_SOURCE_DIR}/yaml-cpp/build")

list(APPEND THIRDPARTY_LIBS ${YAML_LIBRARY})


list(APPEND THIRDPARTY_LIBS pmem)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/oneTBB/opt/TBB")
find_package(TBB CONFIG REQUIRED)
list(APPEND THIRDPARTY_LIBS TBB::tbb) # Add TBB::tbbmalloc to this list to use tbb::fixed_pool


target_link_libraries(myposix PRIVATE ${THIRDPARTY_LIBS})

set_target_properties(myposix PROPERTIES PUBLIC_HEADER ${CMAKE_SOURCE_DIR}/include/myposix.h)
    
include(GNUInstallDirs)

# install(FILES ${CMAKE_SOURCE_DIR}/src/tiering/tiering.h
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# install(FILES ${CMAKE_SOURCE_DIR}/src/tiering/trivial.h
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# install(FILES ${CMAKE_SOURCE_DIR}/src/tiering/thread_test.h
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

#     install(FILES ${CMAKE_SOURCE_DIR}/src/spinlock.h
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS myposix
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

