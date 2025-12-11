#include "cache.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>

MessageCache::MessageCache(int cap) 
    : capacity(cap), head(0), tail(0), size(0), hits(0), misses(0) {
    if (capacity <= 0) {
        throw std::invalid_argument("Cache capacity must be positive");
    }
    cache.resize(capacity);
}

MessageCache::~MessageCache() {
    // Cleanup handled by vector destructor
}

std::string MessageCache::generate_message_id(const std::string& sender, time_t timestamp) const {
    std::stringstream ss;
    ss << sender << "_" << timestamp;
    return ss.str();
}

int MessageCache::find_lru_index() const {
    if (size == 0) {
        return 0;
    }
    
    int lru_index = 0;
    time_t oldest_access = cache[0].last_access;
    
    for (int i = 1; i < size; ++i) {
        if (cache[i].valid && cache[i].last_access < oldest_access) {
            oldest_access = cache[i].last_access;
            lru_index = i;
        }
    }
    
    return lru_index;
}

bool MessageCache::insert(const std::string& sender, const std::string& content, time_t timestamp) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex);
    
    std::string msg_id = generate_message_id(sender, timestamp);
    
    // Check if already exists
    if (index_map.find(msg_id) != index_map.end()) {
        return false;
    }
    
    int insert_index;
    
    if (size < capacity) {
        // Cache not full, use next available slot
        insert_index = size;
        size++;
    } else {
        // Cache full, evict LRU entry
        insert_index = find_lru_index();
        
        // Remove old entry from index map
        if (cache[insert_index].valid) {
            index_map.erase(cache[insert_index].message_id);
        }
    }
    
    // Insert new entry
    cache[insert_index].message_id = msg_id;
    cache[insert_index].content = content;
    cache[insert_index].sender = sender;
    cache[insert_index].timestamp = timestamp;
    cache[insert_index].last_access = time(nullptr);
    cache[insert_index].access_count = 1;
    cache[insert_index].valid = true;
    
    index_map[msg_id] = insert_index;
    
    return true;
}

bool MessageCache::lookup(const std::string& message_id, std::string& content) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex);
    
    auto it = index_map.find(message_id);
    if (it != index_map.end()) {
        int index = it->second;
        if (index >= 0 && index < size && cache[index].valid) {
            content = cache[index].content;
            const_cast<MessageCache*>(this)->hits++;
            return true;
        }
    }
    
    const_cast<MessageCache*>(this)->misses++;
    return false;
}

void MessageCache::update_access(const std::string& message_id) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex);
    
    auto it = index_map.find(message_id);
    if (it != index_map.end()) {
        int index = it->second;
        if (index >= 0 && index < size && cache[index].valid) {
            cache[index].last_access = time(nullptr);
            cache[index].access_count++;
        }
    }
}

uint64_t MessageCache::get_hits() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex);
    return hits;
}

uint64_t MessageCache::get_misses() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex);
    return misses;
}

double MessageCache::get_hit_rate() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex);
    uint64_t total = hits + misses;
    if (total == 0) return 0.0;
    return (static_cast<double>(hits) / total) * 100.0;
}

int MessageCache::get_size() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex);
    return size;
}

void MessageCache::clear() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex);
    
    for (int i = 0; i < size; ++i) {
        cache[i].valid = false;
        cache[i].message_id.clear();
        cache[i].content.clear();
        cache[i].sender.clear();
    }
    
    index_map.clear();
    size = 0;
    hits = 0;
    misses = 0;
}