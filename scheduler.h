#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "common.h"
#include <string>
#include <mutex>
#include <memory>

struct ScheduledClient {
    int socket_fd;
    std::string user_id;
    time_t last_scheduled;
    ScheduledClient* next;
    
    ScheduledClient(int fd, const std::string& id) 
        : socket_fd(fd), user_id(id), last_scheduled(0), next(nullptr) {}
};

/**
 * Round-robin scheduler for fair client message processing
 * Maintains a circular linked list of clients and schedules them in order
 */
class RoundRobinScheduler {
private:
    ScheduledClient* head;
    ScheduledClient* current;
    int client_count;
    mutable std::mutex scheduler_mutex;
    int time_quantum_ms;
    
    // Helper method to find client node
    ScheduledClient* find_client(int socket_fd);

public:
    explicit RoundRobinScheduler(int quantum_ms = TIME_QUANTUM_MS);
    ~RoundRobinScheduler();
    
    // Delete copy constructor and assignment operator
    RoundRobinScheduler(const RoundRobinScheduler&) = delete;
    RoundRobinScheduler& operator=(const RoundRobinScheduler&) = delete;
    
    void add_client(int socket_fd, const std::string& user_id);
    void remove_client(int socket_fd);
    ScheduledClient* get_next_client();
    int get_client_count() const;
    void print_schedule() const;
    
    // Get time quantum
    int get_time_quantum() const { return time_quantum_ms; }
};

#endif