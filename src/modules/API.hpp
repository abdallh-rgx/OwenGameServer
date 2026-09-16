#pragma once
#include "pch.h"
#include <string>

class API {
public:
    static size_t CallBack(void* ptr, size_t size, size_t nmemb, std::string* data);
    static void SendGet(const std::string& url);
    static std::string GetResponse(const std::string& url);
    static std::string GameServer(const std::string& url, const std::string& ip, int port);
};
