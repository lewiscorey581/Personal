#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <cstdint>
#include <ctime>
#include <cstring>

// Server configuration
constexpr int SERVER_PORT = 8080;
constexpr int MAX_CLIENTS = 50;
constexpr int THREAD_POOL_SIZE = 6;
constexpr int BUFFER_SIZE = 4096;
constexpr int CACHE_SIZE = 10;
constexpr int TIME_QUANTUM_MS = 100;
constexpr int USERNAME_MAX_LEN = 63;  // 64 - 1 for null terminator

// Message types
enum class MessageType : uint8_t {
    TEXT = 0x01,
    JOIN = 0x02,
    LEAVE = 0x03,
    AUDIO = 0x04,
    VIDEO = 0x05,
    STATUS = 0x06,
    CACHE_TEST = 0x07  // New type for cache testing
};

// Legacy defines for backward compatibility
#define MSG_TEXT static_cast<uint8_t>(MessageType::TEXT)
#define MSG_JOIN static_cast<uint8_t>(MessageType::JOIN)
#define MSG_LEAVE static_cast<uint8_t>(MessageType::LEAVE)
#define MSG_AUDIO static_cast<uint8_t>(MessageType::AUDIO)
#define MSG_VIDEO static_cast<uint8_t>(MessageType::VIDEO)
#define MSG_STATUS static_cast<uint8_t>(MessageType::STATUS)
#define MSG_CACHE_TEST static_cast<uint8_t>(MessageType::CACHE_TEST)

// Message structure with better memory alignment
struct Message {
    uint8_t type;
    uint8_t padding1[3];  // Explicit padding for alignment
    uint32_t user_id;
    uint32_t payload_size;
    char sender[64];
    char payload[BUFFER_SIZE];
    time_t timestamp;
    
    Message() : type(0), user_id(0), payload_size(0), timestamp(0) {
        memset(padding1, 0, sizeof(padding1));
        memset(sender, 0, sizeof(sender));
        memset(payload, 0, sizeof(payload));
    }
    
    // Helper method to safely set sender
    void set_sender(const std::string& name) {
        strncpy(sender, name.c_str(), sizeof(sender) - 1);
        sender[sizeof(sender) - 1] = '\0';  // Ensure null termination
    }
    
    // Helper method to safely set payload
    void set_payload(const std::string& content) {
        strncpy(payload, content.c_str(), sizeof(payload) - 1);
        payload[sizeof(payload) - 1] = '\0';  // Ensure null termination
        payload_size = strlen(payload);
    }
};

// Cache entry structure
struct CacheEntry {
    std::string message_id;
    std::string content;
    std::string sender;
    time_t timestamp;
    time_t last_access;
    int access_count;
    bool valid;
    
    CacheEntry() : timestamp(0), last_access(0), access_count(0), valid(false) {}
};

// Client information
struct ClientInfo {
    int socket_fd;
    std::string user_id;
    time_t connect_time;
    time_t last_active;
    bool active;
    
    ClientInfo() : socket_fd(-1), connect_time(0), last_active(0), active(false) {}
};

// Performance metrics
struct PerformanceMetrics {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t page_faults_minor;
    uint64_t page_faults_major;
    int active_threads;
    int active_clients;
    
    PerformanceMetrics() : messages_sent(0), messages_received(0), 
                           cache_hits(0), cache_misses(0),
                           page_faults_minor(0), page_faults_major(0),
                           active_threads(0), active_clients(0) {}
};

#endif