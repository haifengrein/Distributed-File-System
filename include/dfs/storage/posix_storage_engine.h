#ifndef DFS_POSIX_STORAGE_ENGINE_H
#define DFS_POSIX_STORAGE_ENGINE_H

#include "dfs/storage/storage_engine_if.h"

namespace dfs {
namespace storage {

class PosixStorageEngine : public IStorageEngine {
public:
    PosixStorageEngine() = default;
    ~PosixStorageEngine() override = default;

    bool Exists(const std::string& path) override;
    void Write(const std::string& path, const std::string& data, bool append = false) override;
    std::string Read(const std::string& path, size_t offset, size_t count) override;
    void Delete(const std::string& path) override;
    struct stat Stat(const std::string& path) override;
    void UpdateMTime(const std::string& path, time_t mtime) override;
    std::vector<std::string> List(const std::string& dir_path) override;
};

} // namespace storage
} // namespace dfs

#endif // DFS_POSIX_STORAGE_ENGINE_H
