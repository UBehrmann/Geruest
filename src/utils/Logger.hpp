/**
 * @file Logger.hpp
 * @date 16.05.2024
 *
 * @author Urs Behrmann
 *
 * @brief Logger class to log messages and errors to terminal and file
 */

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <algorithm>
#include <string>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <fstream>
#include "db/SQLiteInterfaceSingleton.hpp"
#include <vector>

#define LOGS "logs"
#define LOG_TYPES "logTypes"

class Logger {
private:
    bool logToTerminal = true;

    // Private constructor to prevent instantiation
    Logger() {}

    // Delete copy constructor and assignment operator
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static std::string getDateTime() {
        auto now = std::time(nullptr);
        auto localTime = *std::localtime(&now);

        std::ostringstream oss;
        oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");

        return oss.str();
    }

    static std::string removeInvalidChars(const std::string& input) {
        std::string output;
        output.reserve(input.size()); // Reserve space to avoid multiple allocations

        std::copy_if(input.begin(), input.end(), std::back_inserter(output), [](char c) {
            return static_cast<unsigned char>(c) >= 0x20 && static_cast<unsigned char>(c) <= 0x7E && c != '"';
        });

        return output;
    }

    static std::string getOrInsertLogType(SQLiteInterfaceSingleton& db, const std::string& logType) {
        SQLiteTable result = db.readAll(LOG_TYPES, "where logType = '" + logType + "'");

        if (result.empty()) {
            SQLiteLine toInsert = {
                {"logType", logType}
            };

            db.insert(LOG_TYPES, toInsert);

            result = db.readAll(LOG_TYPES, "where logType = '" + logType + "'");
        }

        return result[0][0].second;
    }

    static void logToDB(const std::string& logType, const std::string& message, const std::string& IP = "x.x.x.x") {
        SQLiteInterfaceSingleton& db = SQLiteInterfaceSingleton::getInstance("");

        std::string logTypeId = getOrInsertLogType(db, logType);

        const std::vector<std::pair<std::string, std::string>> toLog = {
            {"logTypeID", logTypeId},
            {"ip", IP},
            {"message", removeInvalidChars(message)}
        };

        db.insert(LOGS, toLog);
    }

public:
    // Static method to get the instance of the singleton class
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    /**
     * Log a message with client ID
     * @param message The message to log
     * @param IP The IP address of the client
     * @param domain The domain of the client
     */
    void log(const std::string& message, const std::string& IP = "x.x.x.x") const {
        if (logToTerminal) std::cout << getDateTime() + " - " + IP + " : " + message << std::endl;

        logToDB("log", message, IP);
    }

    /**
     * Log an error with client ID
     * @param message The error message to log
     * @param IP The IP address of the client
     * @param domain The domain of the client
     */
    void logError(const std::string& message, const std::string& IP = "x.x.x.x") const {
        if (logToTerminal)
            std::cerr << getDateTime() + " - " + IP
                + " : " + message << std::endl;

        logToDB("errorLog", message, IP);
    }

    void logPages(const std::string& message, const std::string& IP = "x.x.x.x") const {
        if (logToTerminal)
            std::cerr << getDateTime() + " - " + IP
                + " : " + message << std::endl;

        logToDB("pagesLog", message, IP);
    }

    void logAPI(const std::string& message, const std::string& IP = "x.x.x.x") const {
        if (logToTerminal) std::cout << getDateTime() + " - " + IP + " : " + message << std::endl;

        logToDB("apiLog", message, IP);
    }

    void logUser(const std::string& message, const std::string& IP = "x.x.x.x") const {
        if (logToTerminal) std::cout << getDateTime() + " - " + IP + " : " + message << std::endl;

        logToDB("userLog", message, IP);
    }

    void set_log_to_terminal(const bool log_to_terminal) { logToTerminal = log_to_terminal; }
};

#endif //LOGGER_HPP
