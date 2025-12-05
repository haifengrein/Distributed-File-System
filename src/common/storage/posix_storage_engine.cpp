#include "dfs/storage/posix_storage_engine.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <utime.h>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cerrno>

namespace dfs {
namespace storage {

bool PosixStorageEngine::Exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

void PosixStorageEngine::Write(const std::string& path, const std::string& data, bool append) {
    std::ios_base::openmode mode = std::ios::binary | std::ios::out;
    if (append) {
        mode |= std::ios::app;
    } else {
        mode |= std::ios::trunc;
    }

    std::ofstream ofs(path, mode);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + path + " (" + std::strerror(errno) + ")");
    }

    ofs.write(data.data(), data.size());
    if (ofs.fail()) {
        throw std::runtime_error("Failed to write data to file: " + path);
    }
}

std::string PosixStorageEngine::Read(const std::string& path, size_t offset, size_t count) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + path + " (" + std::strerror(errno) + ")");
    }

    ifs.seekg(offset);
    if (ifs.fail()) {
         // If we seek past end, we just return empty
         return "";
    }

    std::vector<char> buffer(count);
    ifs.read(buffer.data(), count);
    
    // We return however many bytes we actually read
    std::streamsize bytes_read = ifs.gcount();
    return std::string(buffer.data(), bytes_read);
}

void PosixStorageEngine::Delete(const std::string& path) {
    if (remove(path.c_str()) != 0) {
        // Check if it failed because it didn't exist? 
        // For idempotency, maybe ignore ENOENT?
        if (errno != ENOENT) {
            throw std::runtime_error("Failed to delete file: " + path + " (" + std::strerror(errno) + ")");
        }
    }
}

struct stat PosixStorageEngine::Stat(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        throw std::runtime_error("Failed to stat file: " + path + " (" + std::strerror(errno) + ")");
    }
    return buffer;
}

void PosixStorageEngine::UpdateMTime(const std::string& path, time_t mtime) {
    struct utimbuf new_times;
    
    // We want to preserve access time if possible, but utimbuf sets both.
    // To do this correctly, we should Stat() first to get atime.
    struct stat current = Stat(path);
    
    new_times.actime = current.st_atime;
    new_times.modtime = mtime;

    if (utime(path.c_str(), &new_times) != 0) {
         throw std::runtime_error("Failed to update mtime: " + path + " (" + std::strerror(errno) + ")");
    }
}

std::vector<std::string> PosixStorageEngine::List(const std::string& dir_path) {
    std::vector<std::string> files;
    DIR* dir = opendir(dir_path.c_str());
    if (dir == nullptr) {
        throw std::runtime_error("Failed to open directory: " + dir_path + " (" + std::strerror(errno) + ")");
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip . and ..
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        files.emplace_back(entry->d_name);
    }
    closedir(dir);
    return files;
}

} // namespace storage
} // namespace dfs
