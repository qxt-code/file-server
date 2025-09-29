#pragma once

#include <string>
#include <map>
#include "types/message.h"

#include "nlohmann/json.hpp"

namespace protocol {

using nlohmann::json;

class ResponseBuilder {
public:
    ResponseBuilder() = default;
    ~ResponseBuilder() = default;
    json build(const std::string& response);
    json buildPutResponse(const std::string& status, const std::string& fileName, int fileSize, const std::string& fileHash);
    json buildGetInitResponse(const std::string& fileName, uint64_t offset, uint64_t fileSize, const std::string& fileHash);
    json buildLargePutInit(const std::string& fileName, uint64_t fileSize, const std::string& fileHash, const std::string& token, const std::string& mode, uint64_t chunkHint);
    json buildLargePutComplete(const std::string& fileName, uint64_t fileSize, const std::string& fileHash, bool hashOk);
    json buildSuccessResponse(const std::string& message);
    json buildErrorResponse(int errorCode, const std::string& errorMessage);
    json buildFileListResponse(const std::map<std::string, std::string>& files);
    json buildUploadResponse(const std::string& fileName, bool success);
    json buildDownloadResponse(const std::string& fileName, bool success);

private:
    std::string createJsonResponse(const std::string& status, const std::string& message);
};

} // namespace protocol