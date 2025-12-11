#include "scheduler.h"
#include <iostream>

RoundRobinScheduler::RoundRobinScheduler(int quantum_ms) 
    : head(nullptr), current(nullptr), client_count(0), time_quantum_ms(quantum_ms) {
    if (quantum_ms <= 0) {
        throw std::invalid_argument("Time quantum must be positive");
    }
    std::cout << "[Scheduler] Initialized with " << time_quantum_ms << "ms time quantum" << std::endl;
}

RoundRobinScheduler::~RoundRobinScheduler() {
    std::lock_guard<std::mutex> lock(scheduler_mutex);
    
    if (!head) return;
    
    ScheduledClient* temp = head;
    ScheduledClient* next_client;
    
    do {
        next_client = temp->next;
        delete temp;
        temp = next_client;
    } while (temp != head);
    
    head = nullptr;
    current = nullptr;
    client_count = 0;
}

void RoundRobinScheduler::add_client(int socket_fd, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(scheduler_mutex);
    
    // Check if client already exists
    if (head) {
        ScheduledClient* temp = head;
        do {
            if (temp->socket_fd == socket_fd) {
                std::cerr << "[Scheduler] Client " << socket_fd << " already exists" << std::endl;
                return;
            }
            temp = temp->next;
        } while (temp != head);
    }
    
    ScheduledClient* new_client = new ScheduledClient(socket_fd, user_id);
    
    if (!head) {
        // First client
        head = new_client;
        head->next = head;  // Point to itself
        current = head;
    } else {
        // Find the last client
        ScheduledClient* last = head;
        while (last->next != head) {
            last = last->next;
        }
        
        // Insert new client at the end
        last->next = new_client;
        new_client->next = head;
    }
    
    client_count++;
    std::cout << "[Scheduler] Added client " << user_id << " (fd: " << socket_fd 
              << "), total clients: " << client_count << std::endl;
}

void RoundRobinScheduler::remove_client(int socket_fd) {
    std::lock_guard<std::mutex> lock(scheduler_mutex);
    
    if (!head) return;
    
    ScheduledClient* temp = head;
    ScheduledClient* prev = nullptr;
    
    // Special case: only one client
    if (head->next == head && head->socket_fd == socket_fd) {
        std::cout << "[Scheduler] Removing last client " << head->user_id 
                  << " (fd: " << socket_fd << ")" << std::endl;
        delete head;
        head = nullptr;
        current = nullptr;
        client_count = 0;
        return;
    }
    
    // Find the last node (to get prev for head)
    ScheduledClient* last = head;
    while (last->next != head) {
        last = last->next;
    }
    
    // Case 1: Removing head
    if (head->socket_fd == socket_fd) {
        last->next = head->next;
        ScheduledClient* old_head = head;
        head = head->next;
        if (current == old_head) {
            current = head;
        }
        std::cout << "[Scheduler] Removed client " << old_head->user_id 
                  << " (fd: " << socket_fd << ")" << std::endl;
        delete old_head;
        client_count--;
        return;
    }
    
    // Case 2: Removing non-head client
    prev = head;
    temp = head->next;
    
    while (temp != head) {
        if (temp->socket_fd == socket_fd) {
            prev->next = temp->next;
            if (current == temp) {
                current = temp->next;
            }
            std::cout << "[Scheduler] Removed client " << temp->user_id 
                      << " (fd: " << socket_fd << ")" << std::endl;
            delete temp;
            client_count--;
            return;
        }
        prev = temp;
        temp = temp->next;
    }
    
    std::cerr << "[Scheduler] Client with fd " << socket_fd << " not found" << std::endl;
}

ScheduledClient* RoundRobinScheduler::get_next_client() {
    std::lock_guard<std::mutex> lock(scheduler_mutex);
    
    if (!current) return nullptr;
    
    ScheduledClient* selected = current;
    selected->last_scheduled = time(nullptr);
    
    // Move to next client for round-robin
    current = current->next;
    
    return selected;
}

int RoundRobinScheduler::get_client_count() const {
    std::lock_guard<std::mutex> lock(scheduler_mutex);
    return client_count;
}

ScheduledClient* RoundRobinScheduler::find_client(int socket_fd) {
    if (!head) return nullptr;
    
    ScheduledClient* temp = head;
    do {
        if (temp->socket_fd == socket_fd) {
            return temp;
        }
        temp = temp->next;
    } while (temp != head);
    
    return nullptr;
}

void RoundRobinScheduler::print_schedule() const {
    std::lock_guard<std::mutex> lock(scheduler_mutex);
    
    if (!head) {
        std::cout << "[Scheduler] No clients scheduled" << std::endl;
        return;
    }
    
    std::cout << "[Scheduler] Current schedule (Round-Robin):" << std::endl;
    ScheduledClient* temp = head;
    int position = 0;
    
    do {
        std::cout << "  [" << position++ << "] " << temp->user_id 
                  << " (fd: " << temp->socket_fd << ")";
        if (temp == current) {
            std::cout << " <- CURRENT";
        }
        std::cout << std::endl;
        temp = temp->next;
    } while (temp != head);
}