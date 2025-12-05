#ifndef DFS_STORAGE_ENGINE_IF_H
#define DFS_STORAGE_ENGINE_IF_H

#include <string>
#include <vector>
#include <sys/stat.h>
#include <optional>

namespace dfs {
namespace storage {

/**
 * Abstract Interface for Storage Operations.
 * Decouples the application logic from the underlying filesystem.
 */
class IStorageEngine {
public:
    virtual ~IStorageEngine() = default;

    // File Operations
    virtual bool Exists(const std::string& path) = 0;
    
    // Write 'data' to 'path'.
    // If 'append' is true, adds to end. If false, overwrites/creates.
    virtual void Write(const std::string& path, const std::string& data, bool append = false) = 0;
    
    // Read 'count' bytes starting at 'offset'.
    virtual std::string Read(const std::string& path, size_t offset, size_t count) = 0;
    
    // Delete a file
    virtual void Delete(const std::string& path) = 0;

    // Metadata Operations
    // Returns standard stat structure. Throws exception if not found/error.
    virtual struct stat Stat(const std::string& path) = 0;

    // Set modification time
    virtual void UpdateMTime(const std::string& path, time_t mtime) = 0;

    // Directory Operations
    // Returns list of filenames in the directory (excluding . and ..)
    virtual std::vector<std::string> List(const std::string& dir_path) = 0;
};

} // namespace storage
} // namespace dfs

#endif // DFS_STORAGE_ENGINE_IF_H
