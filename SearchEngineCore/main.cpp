#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS


#include <uwebsockets/App.h>


#include <locale>
#include <codecvt>

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include "mongodb.h"
using namespace std;

#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct User {
    std::string email;
};



// Function to parse query string parameters
map<string, string> parseQueryString(const string& queryString) {
    map<string, string> params;
    size_t pos = 0;
    while ((pos = queryString.find('&')) != string::npos) {
        string key = queryString.substr(0, pos);
        string value = queryString.substr(pos + 1, queryString.find('=', pos) - pos - 1);
        params[key] = value;
        // queryString = queryString.substr(pos + 1);
    }
    if (!queryString.empty()) {
        string key = queryString.substr(0, queryString.find('='));
        string value = queryString.substr(queryString.find('=') + 1);
        params[key] = value;
    }
    return params;
}

// Function to validate email address
bool isValidEmail(const string& email) {
    // Implement a more robust email validation algorithm if needed
    return email.find('@') != string::npos;
}


std::string getEnvVarValue(const char* envVarName) {
    char* buf = nullptr;
    size_t sz = 0;

    if (_dupenv_s(&buf, &sz, envVarName) == 0 && buf != nullptr) {
        std::string value(buf);
        free(buf);
        return value;
    }
    else {
        return "";
    }
}



bool caseInsensitiveCompare(const std::string& str1, const std::string& str2) {
    // Convert both strings to lowercase for case-insensitive comparison
    std::string lowerStr1 = str1;
    std::transform(lowerStr1.begin(), lowerStr1.end(), lowerStr1.begin(), ::tolower);

    std::string lowerStr2 = str2;
    std::transform(lowerStr2.begin(), lowerStr2.end(), lowerStr2.begin(), ::tolower);

    // Compare the lowercase strings
    return lowerStr1 == lowerStr2;
}



#include <iostream>
#include <string>


char* get_body()
{
    char* content_length = getenv("CONTENT_LENGTH");
    if (!content_length)
        return nullptr;
    int size = atoi(content_length);
    if (size == 0)
        return nullptr;
    char* body = (char*)calloc(size + 1, sizeof(char));
    if (body != nullptr)
        std::fread(body, size, 1, stdin);
    else return nullptr;
    return body;
}
// (int argc, char* argv[], char* envp[])


std::string loadFile(const std::string& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Add these helper functions at the top of the file with other utility functions
std::string getMimeType(const std::string& path) {
    const std::map<std::string, std::string> mimeTypes = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"}
    };

    size_t dot_pos = path.find_last_of(".");
    if (dot_pos != std::string::npos) {
        std::string ext = path.substr(dot_pos);
        auto it = mimeTypes.find(ext);
        if (it != mimeTypes.end()) {
            return it->second;
        }
    }
    return "application/octet-stream";
}

bool loadStaticFile(const std::string& basePath, const std::string& requestPath, std::string& content, std::string& mimeType) {
    // Remove leading slash and sanitize path
    std::string relativePath = requestPath;
    if (!relativePath.empty() && relativePath[0] == '/') {
        relativePath = relativePath.substr(1);
    }

    // Construct full file path
    std::string fullPath = basePath + "/" + relativePath;

    // Basic security check to prevent directory traversal
    if (fullPath.find("..") != std::string::npos) {
        return false;
    }

    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Read file content
    content = std::string(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    // Set mime type
    mimeType = getMimeType(fullPath);
    
    return true;
}

int main() {
    // Pre-load index.html
    std::string indexHtml = loadFile("public/index.html");

    uWS::App()
        // Serve index.html at root
        .get("/", [indexHtml](auto* res, auto* req) {
            res->writeHeader("Content-Type", "text/html; charset=utf-8");
            res->end(indexHtml);
        })
        
        // Handle all static files
        .get("/*", [](auto* res, auto* req) {
            std::string path = std::string(req->getUrl());
            std::string content;
            std::string mimeType;

            // Try to load the file from public directory
            if (loadStaticFile("public", path, content, mimeType)) {
                res->writeHeader("Content-Type", mimeType);
                res->end(content);
            }
            else {
                // File not found
                res->writeStatus("404 Not Found");
                res->writeHeader("Content-Type", "text/plain");
                res->end("File not found");
            }
        })

        // Handle email subscription endpoint
        .post("/v2/email-subscribe", [](auto* res, auto* req) {
        // Read the POST body
        std::string buffer;
        res->onData([res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
            buffer.append(data.data(), data.length());

            if (last) {
                try {
                    // Parse JSON body
                    json j = json::parse(buffer);
                    std::string email = j["email"].get<std::string>();

                    // Validate email
                    if (!isValidEmail(email)) {
                        res->writeStatus("400 Bad Request");
                        res->writeHeader("Content-Type", "application/json");
                        res->end("{\"error\": \"Invalid email address\"}");
                        return;
                    }

                    // Subscribe email to mailing list
                    auto result = mongodb().subscribeEmail(email);

                    if (result.success) {
                        res->writeStatus("200 OK");
                        res->writeHeader("Content-Type", "application/json");
                        res->end("{\"message\": \"" + result.message + "\"}");
                    }
                    else {
                        res->writeStatus("400 Bad Request");
                        res->writeHeader("Content-Type", "application/json");
                        res->end("{\"error\": \"" + result.message + "\"}");
                    }

                }
                catch (const std::exception& e) {
                    res->writeStatus("400 Bad Request");
                    res->writeHeader("Content-Type", "application/json");
                    res->end("{\"error\": \"Invalid JSON data: " + std::string(e.what()) + "\"}");
                }
            }
            });

        // Handle upload errors
        res->onAborted([]() {
            std::cout << "Request was aborted" << std::endl;
            });
            })

        .listen(3000, [](auto* listen_socket) {
        if (listen_socket) {
            std::cout << "Server listening on port 3000" << std::endl;
        }
        else {
            std::cout << "Failed to listen on port 3000" << std::endl;
        }
            })
        .run();

    return 0;
}

