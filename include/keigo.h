/**
 * @file keigo.h
 * @brief Multi-tier Storage POSIX Interface Library
 * 
 * Keigo is a high-performance POSIX-compatible wrapper library designed for 
 * key-value storage systems with multi-tier storage architecture. It provides 
 * intelligent file placement, caching, and file recycling mechanisms for SST 
 * (Sorted String Table) files and WAL (Write-Ahead Log) files across different 
 * storage tiers (e.g., persistent memory, SSD, HDD).
 * 
 * The library intercepts standard POSIX file operations and routes them through
 * specialized file environments based on file type, access pattern, and storage
 * tier configuration. It supports both traditional POSIX file operations and
 * persistent memory (PMEM) optimizations.
 * 
 * Key Features:
 * - Multi-tier storage management with automated file placement
 * - File recycling and reuse to reduce allocation overhead
 * - Thread-aware compaction and flush operation handling
 * - Persistent memory optimizations for high-performance workloads
 * - Compatible with RocksDB and similar LSM-tree storage engines
 * 
 * @author Keigo Development Team
 * @version 1.0
 * @date 2024
 * 
 * @see README.md for usage examples and integration guide
 */

#ifndef KEIGO_H
#define KEIGO_H

#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <memory>
#include <vector>
#include <thread>
#include <map>
#include <mutex>
#include <string>
#include <list>
#include <shared_mutex>
#include <vector>
#include <queue>
#include <thread>
#include <atomic>

// ============================================================================
// TYPE DEFINITIONS AND ENUMERATIONS
// ============================================================================

/**
 * @brief Thread operation types for LSM-tree compaction levels and system operations
 * 
 * Defines the different types of background operations that threads can perform
 * in an LSM-tree storage engine. Used for thread registration and operation tracking.
 */
enum op_type {
  THREAD_COMP_L1 = 1,   /**< Compaction thread for Level 1 */
  THREAD_COMP_L2 = 2,   /**< Compaction thread for Level 2 */
  THREAD_COMP_L3 = 3,   /**< Compaction thread for Level 3 */
  THREAD_COMP_L4 = 4,   /**< Compaction thread for Level 4 */
  THREAD_COMP_L5 = 5,   /**< Compaction thread for Level 5 */
  THREAD_COMP_L6 = 6,   /**< Compaction thread for Level 6 */
  THREAD_COMP_L7 = 7,   /**< Compaction thread for Level 7 */
  THREAD_COMP_L8 = 8,   /**< Compaction thread for Level 8 */
  THREAD_COMP_L9 = 9,   /**< Compaction thread for Level 9 */
  THREAD_COMP_L10 = 10, /**< Compaction thread for Level 10 */
  THREAD_FLUSH = 0,     /**< Flush thread (memtable to L0) */
  THREAD_OTHER = -1     /**< Other/unspecified thread type */
};

/**
 * @brief Storage device types for multi-tier storage architecture
 * 
 * Identifies different storage device types with varying performance
 * characteristics for intelligent file placement.
 */
enum device_type {
  SSD = 0,    /**< Solid State Drive */
  NVMM = 1,   /**< Non-Volatile Memory Module (Persistent Memory) */
  NONE = -1   /**< No specific device type */
};

/**
 * @brief File access type classification for storage optimization
 * 
 * Categorizes file access patterns to enable specialized handling
 * for different file types in the storage system.
 */
enum FileAccessType {
    SST_Read = 0,        /**< Reading from SST (Sorted String Table) files */
    SST_Write = 1,       /**< Writing to SST files */
    WAL_Read = 2,        /**< Reading from WAL (Write-Ahead Log) files */
    WAL_Write = 3,       /**< Writing to WAL files */
    ACCESSOR_OTHER = 4,  /**< Other file access types */
};

/**
 * @brief File copy direction for tier migration
 * 
 * Specifies the direction of file movement between storage tiers
 * during background compaction or tiering operations.
 */
enum copy_type {
  NVMM_TO_SSD = 0,  /**< Copy from persistent memory to SSD */
  SSD_TO_NVMM = 1,  /**< Copy from SSD to persistent memory */
  NO_COPY = -1      /**< No copy operation needed */
};

/**
 * @brief Context information for file operations
 * 
 * Provides context about file access patterns and storage preferences
 * to guide the library's decision-making for file placement and handling.
 * 
 * @example
 * ```cpp
 * // Create context for reading an SST file with PMEM optimization
 * auto ctx = std::make_shared<Context>(SST_Read, true);
 * int fd = k_open("/path/to/file.sst", O_RDONLY, ctx);
 * ```
 */
class Context {
   public:
    FileAccessType type_;  /**< Type of file access (SST/WAL read/write) */
    bool is_pmem_;        /**< Whether to use persistent memory optimizations */
    
    /**
     * @brief Construct a new Context object
     * 
     * @param type The file access type (SST_Read, SST_Write, WAL_Read, WAL_Write)
     * @param is_pmem Whether to enable persistent memory optimizations
     */
    Context(FileAccessType type, bool is_pmem) : 
        type_(type), is_pmem_(is_pmem) {}
};

// ============================================================================
// LIBRARY MANAGEMENT FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the tiering library
 * 
 * Must be called before any other library functions. Initializes internal
 * data structures, loads configuration, and sets up file descriptor management.
 * 
 * @note Call this function once at the beginning of your application.
 */
void init_tiering_lib();

/**
 * @brief Cleanup and shutdown the tiering library
 * 
 * Performs cleanup operations, flushes any pending data, and releases
 * resources. Should be called before application termination.
 */
void end_tiering_lib();

/**
 * @brief Activate the library functionality
 * 
 * Enables the library's file interception and management features.
 * Can be used to temporarily enable/disable library operations.
 */
void activate();

/**
 * @brief Check if the library should run now
 * 
 * @return true if the library is active and should process operations
 * @return false if the library is inactive
 */
bool getRunNow();

// ============================================================================
// THREAD AND COMPACTION MANAGEMENT
// ============================================================================

/**
 * @brief Register a thread with a specific operation type
 * 
 * Registers a thread ID with its operation type to enable context-aware
 * file placement decisions based on the thread's role in the storage system.
 * 
 * @param thread_id The pthread ID of the thread to register
 * @param type The operation type (flush, compaction level, etc.)
 * 
 * @example
 * ```cpp
 * registerThread(pthread_self(), THREAD_FLUSH);
 * registerThread(pthread_self(), THREAD_COMP_L1);
 * ```
 */
void registerThread(pthread_t thread_id, op_type type);

/**
 * @brief Register the start of a compaction operation
 * 
 * Notifies the library that a compaction operation is starting,
 * providing the target level for intelligent file placement.
 * 
 * @param compaction_id The pthread ID of the compaction thread
 * @param new_level The target level for the compaction output
 */
void registerStartCompaction(pthread_t compaction_id, int new_level);

/**
 * @brief Get the operation type for a given thread
 * 
 * @param thread_id The pthread ID to query
 * @return op_type The operation type of the thread
 */
op_type getThreadType(pthread_t thread_id);

// ============================================================================
// FILE MANAGEMENT AND TRIVIAL MOVES
// ============================================================================

/**
 * @brief Add an SST file number to level mapping
 * 
 * Associates an SST file number with its level in the LSM-tree
 * for proper tier placement decisions.
 * 
 * @param sst_file_number The SST file identifier
 * @param level The LSM-tree level (0-10)
 */
void add_sst_level(int sst_file_number, int level);

/**
 * @brief Enqueue a trivial move operation
 * 
 * Schedules a file to be moved between levels without rewriting,
 * potentially involving tier migration.
 * 
 * @param filename The name of the file to move
 * @param input_level The source level
 * @param output_level The destination level
 */
void enqueueTrivialMove(std::string filename, int input_level, int output_level);

/**
 * @brief Determine if a file is an SST or WAL file
 * 
 * @param filename The filename to analyze
 * @return int 1 for SST files (.sst), 2 for WAL files (.log), 0 for others
 */
int isSSTorWal(std::string filename);

// ============================================================================
// PATH RESOLUTION FUNCTIONS
// ============================================================================

/**
 * @brief Get the actual file path for opening a file
 * 
 * Determines the optimal storage location and returns the actual path
 * where the file should be opened, considering tier policies and context.
 * 
 * @param fname The logical filename
 * @param actualPath_ret [out] The actual path where the file should be opened
 * @param context File access context information
 * @return true if successful, false otherwise
 */
bool getFileActualPathOpen(std::string fname, std::string &actualPath_ret, std::shared_ptr<Context>& context);

/**
 * @brief Get the actual file path for a given logical filename
 * 
 * @param fname The logical filename
 * @return std::string The actual path where the file is located
 */
std::string getFileActualPath(std::string fname);

/**
 * @brief Get the actual file path for unlinking a file
 * 
 * @param fname The logical filename to unlink
 * @param context File access context information
 * @return std::string The actual path of the file to unlink
 */
std::string getFileActualPathUnlink(std::string fname, std::shared_ptr<Context>& context);

// ============================================================================
// CACHING AND BACKGROUND THREADS
// ============================================================================

/**
 * @brief Caching thread function
 * 
 * Background thread function for managing file caching operations.
 * 
 * @param ptr Thread parameters
 * @return void* Thread return value
 */
void* caching_thread(void *ptr);

/**
 * @brief Alternative caching thread function
 * 
 * @param ptr Thread parameters
 * @return void* Thread return value
 */
void* caching_thread_c(void *ptr);

/**
 * @brief Thread function for monitoring hit ratio statistics
 * 
 * @param ptr Thread parameters
 * @return void* Thread return value
 */
void* check_hitratio(void* ptr);

/**
 * @brief Stop caching operations
 * 
 * Signals caching threads to stop their operations gracefully.
 */
void stopCacheCondition();

// ============================================================================
// POSIX FILE OPERATIONS
// ============================================================================

int k_unlink(const char *pathname, std::shared_ptr<Context> ctx);
int k_open(const char *pathname, int flags, std::shared_ptr<Context> ctx);
int k_open(const char *pathname, int flags, mode_t mode, std::shared_ptr<Context> ctx);
int k_close(int fd);
void *k_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
off_t k_lseek(int fd, off_t offset, int whence);
ssize_t k_read(int fd, void *buf, size_t count);
ssize_t k_pread(int fd, void *buf, size_t count, off_t offset);
ssize_t k_write(int fd, const void *buf, size_t count);
int k_fsync(int fd);
int k_fdatasync(int fd);
ssize_t k_readahead(int fd, off64_t offset, size_t count);
int k_fcntl(int fd, int cmd, ... /* arg */);
int k_posix_fadvise(int fd, off_t offset, off_t len, int advice);
ssize_t k_pwrite(int fd, const void *buf, size_t count, off_t offset);
int k_ftruncate(int fd, off_t length);
int k_fallocate(int fd, int mode, off_t offset, off_t len);
int k_sync_file_range(int fd, off64_t offset, off64_t nbytes, unsigned int flags);
int k_fstat(int fd, struct stat *buf);

// ============================================================================
// STREAM OPERATIONS
// ============================================================================

size_t k_fread_unlocked(void *ptr, size_t size, size_t n, FILE *stream);
int k_fseek(FILE *stream, long offset, int whence);
int k_fclose(FILE *stream);
FILE *k_fdopen(int fildes, const char *mode);
int k_feof(FILE *stream);
void k_clearerr(FILE *stream);

// ============================================================================
// END OF KEIGO LIBRARY INTERFACE
// ============================================================================

/**
 * @}
 * 
 * @note Integration Example:
 * ```cpp
 * // Initialize the library
 * init_tiering_lib();
 * 
 * // Register threads
 * registerThread(pthread_self(), THREAD_FLUSH);
 * 
 * // Use enhanced file operations
 * auto ctx = std::make_shared<Context>(SST_Write, true);
 * int fd = k_open("/data/file.sst", O_CREAT|O_WRONLY, 0644, ctx);
 * k_write(fd, data, size);
 * k_fsync(fd);
 * k_close(fd);
 * 
 * // Cleanup
 * end_tiering_lib();
 * ```
 * 
 * @warning This library requires proper initialization and thread registration
 *          for optimal performance. Always call init_tiering_lib() before
 *          using any other functions.
 * 
 * @see For complete usage examples and integration guides, refer to:
 *      - README.md in the project root
 *      - yaml-config/ directory for configuration examples
 *      - RocksDB integration example at: 
 *        https://github.com/dsrhaslab/tiered-rocksdb/commit/9fcbfd5abc29153d75bd0a927d74af80eee9b6e7
 */

#endif  // KEIGO_H
