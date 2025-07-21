/**
 * @file FileManagement.hpp
 * @date 19.05.2024
 *
 * @author Urs Behrmann
 *
 * @brief This class is used to manage files and folders.
 */

#ifndef GERUEST_FILEMANAGEMENT_HPP
#define GERUEST_FILEMANAGEMENT_HPP

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>

namespace geruest {

class FileManagement {
public:
	/**
	 * Create a folder
	 * @param path
	 * @return
	 */
	static bool createFolder(const std::string& path);

	/**
	 * Check if a file exists
	 * @param path
	 * @return
	 */
	static bool fileExists(const std::string& path);

	/**
	 * Check if a folder exists
	 *
	 * @param path
	 * @return True if the folder exists, false otherwise
	 */
	static bool createFile(const std::string& path);

	/**
	 * Save a file
	 *
	 * @param pathReceived
	 * @param content
	 * @return True if the file was saved, false otherwise
	 */
	static bool saveFile(const std::string& pathReceived, const std::string& content);

	/**
	 * Delete a file
	 * @param path
	 */
	static void deleteFile(const std::string& path);

	/**
	 * Delete a folder
	 * @param path
	 */
	static void deleteFolder(const std::string& path);

	/**
	 * Test if a file is older than a specified number of hours
	 *
	 * @param filePath
	 * @param hours
	 * @return True if the file is older than the specified number of hours, false otherwise
	 */
	static bool isOlderThan(const std::string &filePath, int hours);
};

}  // namespace geruest

#endif //GERUEST_FILEMANAGEMENT_HPP
