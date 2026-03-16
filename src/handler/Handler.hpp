/**
 * @file Handler.hpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief
 */

#ifndef GERUEST_HANDLER_HPP
#define GERUEST_HANDLER_HPP

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
// Define ssize_t for MSVC
typedef SSIZE_T ssize_t;
#endif
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  // For close
#endif

#include <cstring>  // For memset
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"
#include "data/ServerData.hpp"

// Max packet size
#define BUFFER_SIZE 8192

namespace geruest {

class Handler {
private:
	static unsigned clientCount;

#ifdef _WIN32
	SOCKET clientSocket;
#else
	int clientSocket;
#endif

	unsigned idling = 0;

	std::istringstream requestStream;

	unsigned int messageCount = 0;

	const ServerData& serverData;

	const std::string IP;

	std::unique_ptr<char[]> buffer;
	ssize_t bufferLength = 0;

	bool readSocket() { return readSocket(buffer.get(), BUFFER_SIZE); }

	bool readSocket(char *bufferToUse, size_t size);

	bool sendSocket(const char *bufferToSend, size_t size) const;

	void sendToLogger(const std::string &message, LogLevel level = LogLevel::Info) const;

	void sendToLoggerPages(const std::string &message) const;

	void sendToLoggerAPI(const std::string &message) const;

	void sendToLoggerUser(const std::string &message) const;

	void sendToLoggerError(const std::string &message) const;

	void handleRequest(HTTPRequest *request);

	void sendFile(const std::string &contentType, const std::string &contentPath,
					  HTTPRequest* httpRequest) const;

	void sendResponse(const std::string &status, const std::string &contentType,
							const std::string &content) const;

	void sendNotFoundResponse(HTTPRequest* httpRequest) const;


	std::string getExtension(const std::string &path) const;

	void removeSearchParameters();

	std::string buildPath(std::string &pathReceived, const std::string &Extension,
	HTTPRequest* httpRequest) const;

	static std::string getContentType(const std::string &extension);

public:

#ifdef _WIN32
	Handler(SOCKET socket, std::string clientIP, const ServerData& serverDataRef);
#else
	Handler(int socket, std::string clientIP, const ServerData& serverDataRef);
#endif

	~Handler();

	void run();
};

}  // namespace geruest

#endif  // GERUEST_HANDLER_HPP