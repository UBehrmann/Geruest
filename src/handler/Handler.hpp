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
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  // For close
#endif

#include <cstring>  // For memset
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#include "data/HTTPRequest.hpp"
#include "data/HTTPResponse.hpp"
#include "data/ServerData.hpp"

// Max packet size
#define BUFFER_SIZE 8192

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

	ServerData *serverData;

	const std::string IP;

	char *buffer = new char[BUFFER_SIZE];
	unsigned int bufferLength = 0;

	bool readSocket() { return readSocket(buffer, BUFFER_SIZE); }

	bool readSocket(char *bufferToUse, size_t size);

	bool sendSocket(const char *bufferToSend, size_t size) const;

	void sendToLogger(const std::string &message) const;

	void sendToLoggerPages(const std::string &message) const;

	void sendToLoggerAPI(const std::string &message) const;

	void sendToLoggerUser(const std::string &message) const;

	void handleRequest(HTTPRequest *request);

	void sendFile(const std::string &contentType, const std::string &contentPath,
					  HTTPRequest* httpRequest) const;

	void sendResponse(const std::string &status, const std::string &contentType,
							const std::string &content) const;


	std::string getExtension(const std::string &path);

	void removeSearchParameters();

	std::string buildPath(std::string &pathReceived, const std::string &Extension,
	HTTPRequest* httpRequest) ;

	static std::string getContentType(const std::string &extension);

public:
#ifdef _WIN32
	Handler(SOCKET socket, std::string IP, ServerData *serverData);
#else
	Handler(int socket, std::string IP, ServerData *serverData);
#endif

	~Handler();

	void run();

	void sendToLoggerError(const std::string &message) const;
};

#endif  // GERUEST_HANDLER_HPP