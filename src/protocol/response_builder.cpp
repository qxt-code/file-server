#include "response_builder.h"
#include "commands.h"
#include "error_codes.h"
#include <string>

#include "common/debug.h"

namespace protocol {

json ResponseBuilder::build(const std::string& response) {
    json resp = {
        {"responseMessage", response}
    };
    return resp;
}

json ResponseBuilder::buildPutResponse(const std::string& status, const std::string& fileName, int fileSize, const std::string& fileHash) {
    json resp = {
        {"status", status},
        {"fileName", fileName},
        {"fileSize", fileSize},
        {"fileHash", fileHash}
    };
    return resp;

}

json ResponseBuilder::buildGetInitResponse(const std::string& fileName, uint64_t offset, uint64_t fileSize, const std::string& fileHash) {
    json resp = {
        {"status", "get_init"},
        {"fileName", fileName},
        {"offset", offset},
        {"fileSize", fileSize},
        {"fileHash", fileHash}
    };
    return resp;
}

json ResponseBuilder::buildLargePutInit(const std::string& fileName, uint64_t fileSize, const std::string& fileHash, const std::string& token, const std::string& mode, uint64_t chunkHint) {
    json resp = {
        {"status", "large_put_init"},
        {"fileName", fileName},
        {"fileSize", fileSize},
        {"fileHash", fileHash},
        {"uploadToken", token},
        {"mode", mode},
        {"chunkHint", chunkHint}
    };
    return resp;
}

json ResponseBuilder::buildLargePutComplete(const std::string& fileName, uint64_t fileSize, const std::string& fileHash, bool hashOk) {
    json resp = {
        {"status", "large_put_complete"},
        {"fileName", fileName},
        {"fileSize", fileSize},
        {"fileHash", fileHash},
        {"hashOk", hashOk}
    };
    return resp;
}

json ResponseBuilder::buildErrorResponse(int errorCode, const std::string& errorMessage) {
    json resp = {
        {"status", "error"},
        {"errorCode", errorCode},
        {"errorMessage", errorMessage}
    };
    return resp;
}



} // namespace protocol