#pragma once

#include <cstdint>

static constexpr uint64_t MAX_MESSAGE_SIZE = 64 * 1024; // 64KB

/*
    * Message structure:
    * +----------------+----------------+----------------+----------------+
    * | Length (2 bytes) | Type (1 byte)  | Reserved (1 byte) | Body (variable length) |
    * +----------------+----------------+----------------+----------------+
    *
    * Length: Length of the message body (excluding header)
    * Type: Type of the message (e.g., request, response, error)
    * Reserved: Reserved for future use
    * Body: Actual message content

    * Message Types:
    * 0x01: REQUEST
    * 0x02: RESPONSE
    * 0x03: ERROR
    * 0x04: NOTIFICATION

    * for REQUEST: ls pwd cd mkdir:
    * {
    *  "command": "ls",
    *   "params": {}
    * }
    * 
    * for REQUEST: put:
    * {
    *  "command": "put",
    *   "params": {
    *       "fileName": "example.txt",
    *       "fileSize": 12345,
    *       "hashCode": "abc123def456"
    *   }
    * }
    * 
    * for RESPONSE:
    * {
    *   "responseMessage": "message content",
    * }
    * 
    * for ERROR:
    * {
    *  "errorCode": 400,
    *   "errorMessage": "Bad Request"
    * }
*/

struct MessageHeader {
    uint16_t length; // Length of the message body
    uint8_t type;   // Type of the message
    uint8_t reserved; // Reserved for future use

    MessageHeader() : length(0), type(0), reserved(0) {}

} __attribute__((packed));

struct Message {
    MessageHeader header;
    uint8_t body[MAX_MESSAGE_SIZE - sizeof(MessageHeader)] = {0}; // Message body

} __attribute__((packed));