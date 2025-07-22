/**
 * @file FileManagement.cpp
 * @date 16.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to manage files and folders.
 */

#include "FileManagement.hpp"

namespace geruest {

bool FileManagement::createFolder(const std::string& path) {
    try {
        // Check if parent folder exists
        std::string parentFolder = path.substr(0, path.find_last_of('/'));
        if (!std::filesystem::exists(parentFolder)) {
            createFolder(parentFolder);
        }

        // Create the folder
        std::filesystem::create_directory(path);
        return true;
    }
    catch (std::filesystem::filesystem_error& e) {
        return false;
    }
}

bool FileManagement::fileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

bool FileManagement::createFile(const std::string& path) {
    // Check if parent folder exists
    std::string parentFolder = path.substr(0, path.find_last_of('/'));
    if (!std::filesystem::exists(parentFolder)) {
        createFolder(parentFolder);
    }

    // Create the file
    std::ofstream file(path);
    return file.is_open();
}

bool FileManagement::saveFile(const std::string& pathReceived, const std::string& content) {
    // Check if file exists
    if (!fileExists(pathReceived))
        createFile(pathReceived);

    std::ofstream fileStream(pathReceived, std::ios::out | std::ios::binary);

    if (!fileStream) return false;

    fileStream << content;

    fileStream.close();

    return true;
}

void FileManagement::deleteFile(const std::string& path) {
    std::filesystem::remove(path);
}

void FileManagement::deleteFolder(const std::string& path) {
    std::filesystem::remove_all(path);
}

bool FileManagement::isOlderThan(const std::string &filePath, int hours) {
    // Structure to hold file status information
    struct stat fileStat {};

    // Attempt to retrieve file info
    if (stat(filePath.c_str(), &fileStat) != 0)
    {
        // stat returns nonzero if it fails to get file information (e.g. file doesn't exist)
        // Decide how you want to handle this error:
        //  - Return false because we can't confirm it's older
        //  - Or throw an exception / log an error
        return false;
    }

    // Current time
    time_t now = time(nullptr);

    // fileStat.st_mtime is the last-modified time of the file (in seconds since the epoch)
    // difftime returns the difference in seconds
    double diffInSeconds = difftime(now, fileStat.st_mtime);

    // Convert the difference to hours
    double diffInHours = diffInSeconds / 3600.0;

    // Return true if the file is older than the specified number of hours
    return (diffInHours > hours);
}

}  // namespace geruest
