#include "cache.h"
#include "common.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

void print_separator() {
    std::cout << std::string(70, '=') << std::endl;
}

void print_test_header(const std::string& test_name) {
    print_separator();
    std::cout << "TEST: " << test_name << std::endl;
    print_separator();
}

void print_cache_stats(const MessageCache& cache) {
    std::cout << "\n[Cache Statistics]" << std::endl;
    std::cout << "  Size: " << cache.get_size() << "/" << cache.get_capacity() << std::endl;
    std::cout << "  Hits: " << cache.get_hits() << std::endl;
    std::cout << "  Misses: " << cache.get_misses() << std::endl;
    std::cout << "  Hit Rate: " << std::fixed << std::setprecision(2) 
              << cache.get_hit_rate() << "%" << std::endl;
}

void test_basic_operations() {
    print_test_header("Basic Cache Operations");
    
    MessageCache cache(5);
    std::string content;
    
    // Test 1: Insert and retrieve
    std::cout << "\n1. Testing insert and lookup..." << std::endl;
    cache.insert("Alice", "Hello World", time(nullptr));
    cache.insert("Bob", "Hi there", time(nullptr) + 1);
    cache.insert("Charlie", "Good morning", time(nullptr) + 2);
    
    std::string msg_id = "Alice_" + std::to_string(time(nullptr));
    bool found = cache.lookup(msg_id, content);
    std::cout << "   Lookup Alice's message: " << (found ? "✓ FOUND" : "✗ NOT FOUND") << std::endl;
    if (found) {
        std::cout << "   Content: \"" << content << "\"" << std::endl;
    }
    
    // Test 2: Duplicate prevention
    std::cout << "\n2. Testing duplicate prevention..." << std::endl;
    bool inserted = cache.insert("Alice", "Hello World", time(nullptr));
    std::cout << "   Re-insert same message: " << (inserted ? "✗ ALLOWED (ERROR)" : "✓ BLOCKED") << std::endl;
    
    print_cache_stats(cache);
}

void test_lru_eviction() {
    print_test_header("LRU Eviction Policy");
    
    MessageCache cache(5);  // Small cache for easy testing
    std::string content;
    time_t base_time = time(nullptr);
    
    std::cout << "\n1. Filling cache to capacity (5 messages)..." << std::endl;
    for (int i = 0; i < 5; i++) {
        std::string sender = "User" + std::to_string(i);
        std::string message = "Message " + std::to_string(i);
        cache.insert(sender, message, base_time + i);
        std::cout << "   Inserted: " << sender << " - \"" << message << "\"" << std::endl;
    }
    
    print_cache_stats(cache);
    
    std::cout << "\n2. Accessing User1's message (updates LRU)..." << std::endl;
    std::string msg_id = "User1_" + std::to_string(base_time + 1);
    cache.lookup(msg_id, content);
    cache.update_access(msg_id);
    std::cout << "   Accessed: User1" << std::endl;
    
    std::cout << "\n3. Inserting new message (should evict User0 - oldest unused)..." << std::endl;
    cache.insert("User5", "New Message", base_time + 10);
    std::cout << "   Inserted: User5 - \"New Message\"" << std::endl;
    
    std::cout << "\n4. Verifying eviction..." << std::endl;
    std::string user0_id = "User0_" + std::to_string(base_time);
    bool found0 = cache.lookup(user0_id, content);
    std::cout << "   Lookup User0 (should be evicted): " << (found0 ? "✗ FOUND (ERROR)" : "✓ NOT FOUND") << std::endl;
    
    std::string user1_id = "User1_" + std::to_string(base_time + 1);
    bool found1 = cache.lookup(user1_id, content);
    std::cout << "   Lookup User1 (should still exist): " << (found1 ? "✓ FOUND" : "✗ NOT FOUND (ERROR)") << std::endl;
    
    print_cache_stats(cache);
}

void test_cache_performance() {
    print_test_header("Cache Performance Test");
    
    MessageCache cache(10);
    time_t base_time = time(nullptr);
    
    std::cout << "\n1. Inserting 10 messages..." << std::endl;
    for (int i = 0; i < 10; i++) {
        std::string sender = "Sender" + std::to_string(i);
        std::string message = "Performance test message " + std::to_string(i);
        cache.insert(sender, message, base_time + i);
    }
    std::cout << "   Cache filled to capacity" << std::endl;
    
    std::cout << "\n2. Performing 50 lookups (mix of hits and misses)..." << std::endl;
    std::string content;
    int successful_lookups = 0;
    
    for (int i = 0; i < 50; i++) {
        int sender_num = i % 15;  // Some will miss (10-14)
        std::string msg_id = "Sender" + std::to_string(sender_num) + "_" + 
                             std::to_string(base_time + sender_num);
        if (cache.lookup(msg_id, content)) {
            successful_lookups++;
        }
    }
    
    std::cout << "   Successful lookups: " << successful_lookups << "/50" << std::endl;
    print_cache_stats(cache);
}

void test_concurrent_access() {
    print_test_header("Concurrent Access Test");
    
    MessageCache cache(20);
    std::atomic<int> insert_count(0);
    std::atomic<int> lookup_count(0);
    time_t base_time = time(nullptr);
    
    std::cout << "\nStarting 4 concurrent threads (2 inserters, 2 readers)..." << std::endl;
    
    auto inserter = [&](int thread_id) {
        for (int i = 0; i < 10; i++) {
            std::string sender = "Thread" + std::to_string(thread_id) + "_User" + std::to_string(i);
            std::string message = "Concurrent message from thread " + std::to_string(thread_id);
            if (cache.insert(sender, message, base_time + thread_id * 100 + i)) {
                insert_count++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    
    auto reader = [&](int thread_id) {
        std::string content;
        for (int i = 0; i < 20; i++) {
            int target_thread = i % 2;
            int target_msg = i % 10;
            std::string msg_id = "Thread" + std::to_string(target_thread) + "_User" + 
                                std::to_string(target_msg) + "_" + 
                                std::to_string(base_time + target_thread * 100 + target_msg);
            if (cache.lookup(msg_id, content)) {
                lookup_count++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    
    std::vector<std::thread> threads;
    threads.emplace_back(inserter, 0);
    threads.emplace_back(inserter, 1);
    threads.emplace_back(reader, 2);
    threads.emplace_back(reader, 3);
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "\n[Results]" << std::endl;
    std::cout << "  Successful inserts: " << insert_count << std::endl;
    std::cout << "  Successful lookups: " << lookup_count << std::endl;
    print_cache_stats(cache);
    std::cout << "\n✓ No crashes or deadlocks detected" << std::endl;
}

void test_edge_cases() {
    print_test_header("Edge Cases and Stress Test");
    
    std::cout << "\n1. Testing empty cache lookup..." << std::endl;
    MessageCache cache1(5);
    std::string content;
    bool found = cache1.lookup("nonexistent_message", content);
    std::cout << "   Lookup in empty cache: " << (found ? "✗ FOUND (ERROR)" : "✓ NOT FOUND") << std::endl;
    
    std::cout << "\n2. Testing cache of size 1..." << std::endl;
    MessageCache cache2(1);
    cache2.insert("User1", "Message1", time(nullptr));
    cache2.insert("User2", "Message2", time(nullptr) + 1);
    std::cout << "   Cache size after 2 inserts: " << cache2.get_size() << "/1" << std::endl;
    std::cout << "   " << (cache2.get_size() == 1 ? "✓ PASS" : "✗ FAIL") << std::endl;
    
    std::cout << "\n3. Rapid insertions (100 messages into cache of size 10)..." << std::endl;
    MessageCache cache3(10);
    time_t base_time = time(nullptr);
    for (int i = 0; i < 100; i++) {
        std::string sender = "RapidUser" + std::to_string(i);
        std::string message = "Rapid message " + std::to_string(i);
        cache3.insert(sender, message, base_time + i);
    }
    std::cout << "   Final cache size: " << cache3.get_size() << "/10" << std::endl;
    std::cout << "   " << (cache3.get_size() == 10 ? "✓ PASS" : "✗ FAIL") << std::endl;
    
    print_cache_stats(cache3);
}

int main() {
    std::cout << "\n";
    print_separator();
    std::cout << "    MESSAGE CACHE TEST SUITE" << std::endl;
    std::cout << "    Testing LRU Cache Implementation" << std::endl;
    print_separator();
    std::cout << std::endl;
    
    try {
        test_basic_operations();
        std::cout << "\n\n";
        
        test_lru_eviction();
        std::cout << "\n\n";
        
        test_cache_performance();
        std::cout << "\n\n";
        
        test_concurrent_access();
        std::cout << "\n\n";
        
        test_edge_cases();
        std::cout << "\n\n";
        
        print_separator();
        std::cout << "✓ ALL TESTS COMPLETED SUCCESSFULLY" << std::endl;
        print_separator();
        std::cout << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED WITH EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}