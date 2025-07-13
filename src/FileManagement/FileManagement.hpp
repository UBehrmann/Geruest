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

class FileManagement {
public:
	/**
	 * Create a folder
	 * @param path
	 * @return
	 */
	static bool createFolder(const std::string& path) {
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

	/**
	 * Check if a file exists
	 * @param path
	 * @return
	 */
	static bool fileExists(const std::string& path) {
		return std::filesystem::exists(path);
	}

	/**
	 * Check if a folder exists
	 *
	 * @param path
	 * @return True if the folder exists, false otherwise
	 */
	static bool createFile(const std::string& path) {

		// Check if parent folder exists
		std::string parentFolder = path.substr(0, path.find_last_of('/'));
		if (!std::filesystem::exists(parentFolder)) {
			createFolder(parentFolder);
		}

		// Create the file
		std::ofstream file(path);
		return file.is_open();
	}

	/**
	 * Save a file
	 *
	 * @param pathReceived
	 * @param content
	 * @return True if the file was saved, false otherwise
	 */
	static bool saveFile(const std::string& pathReceived, const std::string& content) {

		// Check if file exists
		if (!fileExists(pathReceived))
			createFile(pathReceived);

		std::ofstream fileStream(pathReceived, std::ios::out | std::ios::binary);

		if (!fileStream) return false;

		fileStream << content;

		fileStream.close();

		return true;
	}

	/**
	 * Delete a file
	 * @param path
	 */
	static void deleteFile(const std::string& path) {
		std::filesystem::remove(path);
	}

	/**
	 * Delete a folder
	 * @param path
	 */
	static void deleteFolder(const std::string& path) {
		std::filesystem::remove_all(path);
	}

	/**
	 * Test if a file is older than a specified number of hours
	 *
	 * @param filePath
	 * @param hours
	 * @return True if the file is older than the specified number of hours, false otherwise
	 */
	static bool isOlderThan(const std::string &filePath, int hours)
	{
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
};

#endif //GERUEST_FILEMANAGEMENT_HPP
