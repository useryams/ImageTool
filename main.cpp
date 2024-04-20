#include <curl/curl.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <fstream>
#include <array>
#include <vector>


static CURL* curl;

static const std::array<const std::string, 12> extentions = {
    ".jpeg",
    ".jpg",
    ".png",
    ".gif",
    ".bmp",
    ".ico",
    ".webp",
    ".webm",

    ".pdf",
    ".svg",
    ".eps",
    ".ai"
};

size_t function( void* data, size_t unused, size_t dataSize, void* userdata) {
    std::string* pStr = reinterpret_cast<std::string*>(userdata);
    pStr->reserve(dataSize);

    for (size_t i = 0; i < dataSize; ++i) {
        pStr->push_back(reinterpret_cast<char*>(data)[i]);
    }

    return dataSize;
}

void CurlInit() {

    curl = curl_easy_init();

    CURLcode result = CURLE_OK;

    result = curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
    if (result == CURLE_OK) {

        result = curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl 8.7.1");
        if (result == CURLE_OK) {
            
            result = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, function);
        }
    }



    if (result != CURLE_OK) {
        std::cout << "ERROR: Curl failed to initialise. " << curl_easy_strerror(result) << std::endl;
    }
}

void CurlDestory() {
    curl_easy_cleanup(curl);
}

bool Curl(const std::string url, std::string* pData) {
    
    CURLcode result = CURLE_OK;
    
    result = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (result == CURLE_OK) {

        result = curl_easy_setopt(curl, CURLOPT_WRITEDATA, pData);
        if (result == CURLE_OK) {

            result = curl_easy_perform(curl);
        }
    }

    if (result != CURLE_OK) {
        std::cout << "ERROR: " << url << " " << curl_easy_strerror(result) << std::endl;
        return false;
    }
    
    return true;
}

void SaveImage(std::string url, std::string directory = "images/") {
    
    // Download website data.
    std::string pagedata;
    if (Curl(url, &pagedata))
    {
        // Determine filename.
        std::string filename;
        size_t i = url.rfind('/');
        if (i == std::string::npos){
            filename = url;
        }
        else {
            filename = url.substr(i + 1);
        }

        // Find how many duplicates
        int duplicates = 0;
        while (std::ifstream(directory + filename + (duplicates ? '(' + std::to_string(duplicates) + ')' : "") )) {
            ++duplicates;
        }

        // Write to file.
        std::ofstream o(directory + filename + (duplicates ? '(' + std::to_string(duplicates) + ')' : "") );
        if (o.is_open()) {
            o.write(pagedata.c_str(), pagedata.size());
            o.close();
            std::cout << "Saved " << filename + (duplicates ? '(' + std::to_string(duplicates) + ')' : "") << std::endl;
        }
        else {
            std::cout << "ERROR: Failed to open " << filename << std::endl;
        }
    }
}

void SeperateImgSrc(const std::string url, std::vector<std::string>* pBuffer) {

    // Get domain name.
    size_t protocolSize = sizeof("https://") - 1;
    std::string domain = url.substr(protocolSize, url.find('/', protocolSize) - protocolSize);

    // Download webpage
    std::string webpage;
    std::cout << "Retrieving " << url << std::endl;
    if (Curl(url, &webpage))
    {
        std::cout << "Success. Finding Images..." << std::endl;
    }

    // Debug
    std::ofstream o("webpage.txt");
    o.write(webpage.c_str(), webpage.size());
    o.close();

    // Scrape for images
    for (const std::string& extention : extentions) {

        int chances = 5;
        size_t previous_duplicates = 0;

        size_t i = 0;
        while (i != std::string::npos) {

            // Find file extention
            i = webpage.find(extention, i);
            if (i != std::string::npos) {

                // Find last character of url
                size_t a = webpage.find('\'', i);
                size_t b = webpage.find('\"', i);
                size_t c = webpage.find(' ', i);
                size_t last = std::min(a != std::string::npos ? a : 0, b != std::string::npos ? b : 0);
                last = std::min(last != std::string::npos ? last : 0, c != std::string::npos ? c : 0);


                if (last != std::string::npos) {

                    // Find first charcter of url
                    size_t a = webpage.rfind('\'', last - 1);
                    size_t b = webpage.rfind('\"', last - 1);
                    size_t c = webpage.rfind(' ', last - 1);
                    size_t first = std::max(a != std::string::npos ? a : 0, b != std::string::npos ? b : 0);
                    first = std::max(first != std::string::npos ? first : 0, c != std::string::npos ? c : 0);

                    if (first != std::string::npos) {

                        first += 1;

                        std::string img_url = webpage.substr(first, last - first);

                        std::erase(img_url, '\\');
                        
                        if (img_url.substr(0, 2) == "//") {
                            img_url.insert(0, "https:");
                        }
                        else if (img_url.front() == '/') {
                            img_url.insert(0, "https://" + domain);
                        }

                        if (img_url != extention && img_url.substr(0, 4) == "http")
                        {
                            pBuffer->push_back(img_url);
                            std::cout << "Found " << pBuffer->back() << std::endl;
                        }
                    }

                    i = last;
                }
            }

            // Prevent infinate loop
            if(pBuffer->size() >= 128) {
                size_t sz = pBuffer->size();

                std::sort(pBuffer->begin(), pBuffer->end());
                auto it = std::unique(pBuffer->begin(), pBuffer->end());
                pBuffer->resize(std::distance(pBuffer->begin(), it));

                if (previous_duplicates == sz - pBuffer->size()) {
                    if (chances > 0) {
                        --chances;
                    }
                    else {
                        std::cout << "Stuck in a loop. Breaking..." << std::endl;
                        break;
                    }
                }

                previous_duplicates = sz - pBuffer->size();

                std::cout << "Removed " << previous_duplicates << " duplicates. Continuing..." << std::endl;
            }
        }
    }

    // Remove doubles
    size_t unsortedLength = pBuffer->size();
    
    std::sort(pBuffer->begin(), pBuffer->end());
    auto it = std::unique(pBuffer->begin(), pBuffer->end());
    pBuffer->resize(std::distance(pBuffer->begin(), it));

    std::cout << "Removed " << unsortedLength - pBuffer->size() << " duplicates." << std::endl;
    std::cout << "Buffer contains url to " << pBuffer->size() << " images." << std::endl << std::endl;
}


int main() {
    CurlInit();

    std::vector<std::string> buffer;
    SeperateImgSrc("put url here", &buffer);

    for (const std::string& url : buffer) {
        SaveImage(url);
    }

    CurlDestory();

    return 0;
}
