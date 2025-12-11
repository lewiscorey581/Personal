#ifndef CACHE_H
#define CACHE_H

#include "common.h"
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <string>

/**
 * Thread-safe LRU message cache implementation
 * Uses shared_mutex for reader-writer locking to optimize concurrent reads
 */
class MessageCache {
private:
    std::vector<CacheEntry> cache;
    int capacity;
    int head;  // Not currently used but kept for future circular buffer implementation
    int tail;  // Not currently used but kept for future circular buffer implementation
    int size;
    std::unordered_map<std::string, int> index_map;  // Changed to unordered_map for O(1) lookup
    mutable std::shared_mutex cache_mutex;
    
    uint64_t hits;
    uint64_t misses;
    
    // Private helper methods
    int find_lru_index() const;
    std::string generate_message_id(const std::string& sender, time_t timestamp) const;

public:
    explicit MessageCache(int capacity = CACHE_SIZE);
    ~MessageCache();
    
    // Delete copy constructor and assignment operator (cache should not be copied)
    MessageCache(const MessageCache&) = delete;
    MessageCache& operator=(const MessageCache&) = delete;
    
    // Allow move operations
    MessageCache(MessageCache&&) noexcept = default;
    MessageCache& operator=(MessageCache&&) noexcept = default;
    
    bool insert(const std::string& sender, const std::string& content, time_t timestamp);
    bool lookup(const std::string& message_id, std::string& content) const;
    void update_access(const std::string& message_id);
    
    // Const getters
    uint64_t get_hits() const;
    uint64_t get_misses() const;
    double get_hit_rate() const;
    int get_size() const;
    int get_capacity() const { return capacity; }
    
    // Clear cache (useful for testing)
    void clear();
};

#endif