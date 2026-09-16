#include "pch.h"
#include "API.hpp"
#include "options.h"
#include <curl/curl.h>

size_t API::CallBack(void* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

void API::SendGet(const std::string& url) {
    std::thread([url]() {
        curl_global_init(CURL_GLOBAL_ALL);
        CURL* curl = curl_easy_init();
        if (!curl) { curl_global_cleanup(); return; }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CallBack);

        std::string body;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
    }).detach();
}

std::string API::GetResponse(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CallBack);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) response = "";

        curl_easy_cleanup(curl);
    }
    return response;
}

std::string API::GameServer(const std::string& url, const std::string& ip, int port) {
    CURL* curl = curl_easy_init();
    std::string returning;

    if (curl) {
        std::string responseData = "{ \"ipAddress\": \"" + ip + "\", \"port\": " + std::to_string(port) + " }";

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, responseData.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* contents, size_t size, size_t nmemb, std::string* output) {
            size_t totalSize = size * nmemb;
            output->append((char*)contents, totalSize);
            return totalSize;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &returning);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) returning = "";

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return returning;
}
