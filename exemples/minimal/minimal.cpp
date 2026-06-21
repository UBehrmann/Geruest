// minimal.cpp — static site + /v1/hello (merge/obfuscation off by default; see ../showcase/)
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>
#include "Geruest.hpp"

using namespace geruest;

std::unique_ptr<Geruest> server;

void onSignal(int) {
    if (server) {
        server->stop();
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, onSignal);

    server = std::make_unique<Geruest>();
    const auto exeDir = std::filesystem::canonical("/proc/self/exe").parent_path();
    server->loadConfig((exeDir / ".env").string());
    server->setPort(8080);
    server->setHostname("localhost");

    const auto website = std::filesystem::absolute(
        std::filesystem::path(argv[0]).parent_path() / "website");
    server->addRoot(website.string());

    server->addRoute("/v1/hello", [](const HTTPRequest&) {
        HTTPResponse res("200 OK");
        res.setHeader("Content-Type", "application/json");
        res.setBody(R"({"message":"hello from Geruest"})");
        return res;
    });

    if (!server->init()) {
        return EXIT_FAILURE;
    }

    std::cout << "Minimal server: http://localhost:8080  (Ctrl+C to stop)\n";
    server->start();
    return EXIT_SUCCESS;
}
