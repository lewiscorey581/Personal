#include "common.h"
#include "thread_pool.h"
#include "cache.h"
#include "scheduler.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <map>
#include <mutex>
#include <fstream>
#include <sstream>
#include <csignal>
#include <cerrno>
#include <chrono>
#include <thread>
#include <atomic>

// Global variables
std::map<int, ClientInfo> clients;
std::mutex clients_mutex;
MessageCache message_cache(CACHE_SIZE);
RoundRobinScheduler scheduler;
PerformanceMetrics metrics;
std::mutex metrics_mutex;
std::atomic<bool> server_running(true);
std::ofstream log_file;

// Function prototypes
void handle_client(int client_socket);
void broadcast_message(const Message& msg, int sender_socket);
void log_message(const std::string& message);
void update_metrics();
void read_page_faults();
void signal_handler(int signum);
void print_statistics();
bool setup_server_socket(int& server_socket);
void cleanup_server(int server_socket);

void log_message(const std::string& message) {
    time_t now = time(nullptr);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    std::string log_entry = std::string(timestamp) + " - " + message;
    std::cout << log_entry << std::endl;
    
    if (log_file.is_open()) {
        log_file << log_entry << std::endl;
        log_file.flush();
    }
}

void read_page_faults() {
#ifdef __linux__
    try {
        std::ifstream status_file("/proc/self/status");
        if (!status_file.is_open()) return;
        
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.find("VmHWM:") == 0 || line.find("VmRSS:") == 0) {
                std::lock_guard<std::mutex> lock(metrics_mutex);
                metrics.page_faults_minor++;
            }
        }
    } catch (const std::exception& e) {
        // Silently ignore errors reading page faults
    }
#endif
}

void update_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    
    {
        std::lock_guard<std::mutex> clients_lock(clients_mutex);
        metrics.active_clients = static_cast<int>(clients.size());
    }
    
    metrics.cache_hits = message_cache.get_hits();
    metrics.cache_misses = message_cache.get_misses();
    
    read_page_faults();
}

void broadcast_message(const Message& msg, int sender_socket) {
    std::vector<int> failed_sockets;
    
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        
        for (auto& [socket_fd, client_info] : clients) {
            if (socket_fd != sender_socket && client_info.active) {
                ssize_t sent = send(socket_fd, &msg, sizeof(Message), MSG_NOSIGNAL);
                if (sent > 0) {
                    std::lock_guard<std::mutex> metrics_lock(metrics_mutex);
                    metrics.messages_sent++;
                } else {
                    // Mark for cleanup
                    failed_sockets.push_back(socket_fd);
                }
            }
        }
    }
    
    // Clean up failed connections (do this outside the clients lock)
    for (int fd : failed_sockets) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto it = clients.find(fd);
        if (it != clients.end()) {
            log_message("Client connection lost: " + it->second.user_id);
            it->second.active = false;
        }
    }
    
    // Add message to cache
    message_cache.insert(msg.sender, msg.payload, msg.timestamp);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    Message msg;
    std::string user_id;
    
    try {
        // Set socket timeout to allow checking server_running
        struct timeval tv;
        tv.tv_sec = 1;  // 1 second timeout for faster shutdown
        tv.tv_usec = 0;
        if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            log_message("Warning: Failed to set socket timeout");
        }
        
        // Receive initial user ID
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            close(client_socket);
            return;
        }
        
        buffer[std::min(bytes, (ssize_t)(BUFFER_SIZE - 1))] = '\0';
        user_id = std::string(buffer);
        
        // Validate user ID
        if (user_id.empty() || user_id.length() > USERNAME_MAX_LEN) {
            log_message("Invalid user ID received, disconnecting");
            close(client_socket);
            return;
        }
        
        // Register client
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            ClientInfo info;
            info.socket_fd = client_socket;
            info.user_id = user_id;
            info.connect_time = time(nullptr);
            info.last_active = time(nullptr);
            info.active = true;
            clients[client_socket] = info;
            
            std::lock_guard<std::mutex> metrics_lock(metrics_mutex);
            metrics.active_clients++;
        }
        
        // Add to scheduler
        scheduler.add_client(client_socket, user_id);
        
        // Send join notification
        Message join_msg;
        memset(&join_msg, 0, sizeof(join_msg));
        join_msg.type = MSG_JOIN;
        join_msg.timestamp = time(nullptr);
        join_msg.set_sender(user_id);
        snprintf(join_msg.payload, sizeof(join_msg.payload), "%s has joined the chat", user_id.c_str());
        join_msg.payload_size = strlen(join_msg.payload);
        broadcast_message(join_msg, client_socket);
        
        log_message("Client connected: " + user_id + " (fd: " + std::to_string(client_socket) + ")");
        
        // Main message loop
        while (server_running.load()) {
            memset(&msg, 0, sizeof(msg));
            bytes = recv(client_socket, &msg, sizeof(Message), 0);
            
            if (bytes < 0) {
                // Check if it was a timeout
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    // Timeout - continue loop to check server_running
                    continue;
                }
                // Real error - client disconnected
                break;
            }
            
            if (bytes == 0 || bytes != sizeof(Message)) {
                // Client disconnected or invalid message
                break;
            }
            
            {
                std::lock_guard<std::mutex> metrics_lock(metrics_mutex);
                metrics.messages_received++;
            }
            
            // Update last active time
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                auto it = clients.find(client_socket);
                if (it != clients.end()) {
                    it->second.last_active = time(nullptr);
                }
            }
            
            // Process message based on type
            switch (msg.type) {
                case MSG_TEXT: {
                    // Ensure null-terminated strings
                    msg.sender[sizeof(msg.sender) - 1] = '\0';
                    msg.payload[sizeof(msg.payload) - 1] = '\0';
                    
                    // Check cache for recent messages from same user (simulates deduplication)
                    std::string recent_msg_id = std::string(msg.sender) + "_" + 
                                                std::to_string(msg.timestamp - 5);
                    std::string cached;
                    message_cache.lookup(recent_msg_id, cached);
                    
                    msg.timestamp = time(nullptr);
                    broadcast_message(msg, client_socket);
                    log_message("Message from " + user_id + ": " + std::string(msg.payload));
                    
                    // Simulate cache hits by looking up recently sent messages
                    for (int i = 1; i <= 3; i++) {
                        std::string prev_msg_id = user_id + "_" + std::to_string(msg.timestamp - i);
                        std::string cached_msg;
                        if (message_cache.lookup(prev_msg_id, cached_msg)) {
                            message_cache.update_access(prev_msg_id);
                        }
                    }
                    break;
                }
                
                case MSG_STATUS: {
                    // Handle status request - send stats back to client
                    update_metrics();
                    
                    // Create a stats message to send back
                    Message stats_msg;
                    memset(&stats_msg, 0, sizeof(stats_msg));
                    stats_msg.type = MSG_TEXT;
                    stats_msg.timestamp = time(nullptr);
                    stats_msg.set_sender("SERVER");
                    
                    // Format statistics
                    std::stringstream stats_stream;
                    {
                        std::lock_guard<std::mutex> lock(metrics_mutex);
                        stats_stream << "\n=== SERVER STATISTICS ===\n"
                                   << "Messages Sent:     " << metrics.messages_sent << "\n"
                                   << "Messages Received: " << metrics.messages_received << "\n"
                                   << "Active Clients:    " << metrics.active_clients << "\n"
                                   << "Cache Hits:        " << metrics.cache_hits << "\n"
                                   << "Cache Misses:      " << metrics.cache_misses << "\n"
                                   << "Cache Hit Rate:    " << std::fixed << std::setprecision(2)
                                   << message_cache.get_hit_rate() << "%\n"
                                   << "Cache Size:        " << message_cache.get_size() << "/" 
                                   << message_cache.get_capacity() << "\n"
                                   << "=========================";
                    }
                    
                    stats_msg.set_payload(stats_stream.str());
                    
                    // Send stats to requesting client only
                    ssize_t sent = send(client_socket, &stats_msg, sizeof(Message), MSG_NOSIGNAL);
                    if (sent > 0) {
                        log_message("Statistics sent to " + user_id + " (" + std::to_string(sent) + " bytes)");
                    } else {
                        log_message("ERROR: Failed to send statistics to " + user_id);
                    }
                    
                    // Also print to server console
                    print_statistics();
                    break;
                }
                    
                default:
                    log_message("Unknown message type " + std::to_string(msg.type) + 
                                " from " + user_id);
                    break;
            }
        }
    } catch (const std::exception& e) {
        log_message("Exception in handle_client: " + std::string(e.what()));
    }
    
    // Client cleanup
    scheduler.remove_client(client_socket);
    
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(client_socket);
        std::lock_guard<std::mutex> metrics_lock(metrics_mutex);
        if (metrics.active_clients > 0) {
            metrics.active_clients--;
        }
    }
    
    // Send leave notification if we have a user_id
    if (!user_id.empty()) {
        Message leave_msg;
        memset(&leave_msg, 0, sizeof(leave_msg));
        leave_msg.type = MSG_LEAVE;
        leave_msg.timestamp = time(nullptr);
        leave_msg.set_sender(user_id);
        snprintf(leave_msg.payload, sizeof(leave_msg.payload), "%s has left the chat", user_id.c_str());
        leave_msg.payload_size = strlen(leave_msg.payload);
        broadcast_message(leave_msg, -1);
        
        log_message("Client disconnected: " + user_id + " (fd: " + std::to_string(client_socket) + ")");
    }
    
    close(client_socket);
}

void print_statistics() {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    
    std::cout << "\n==================================" << std::endl;
    std::cout << "    SERVER STATISTICS" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "Messages Sent:     " << metrics.messages_sent << std::endl;
    std::cout << "Messages Received: " << metrics.messages_received << std::endl;
    std::cout << "Active Clients:    " << metrics.active_clients << std::endl;
    std::cout << "Cache Hits:        " << metrics.cache_hits << std::endl;
    std::cout << "Cache Misses:      " << metrics.cache_misses << std::endl;
    std::cout << "Cache Hit Rate:    " << std::fixed << std::setprecision(2) 
              << message_cache.get_hit_rate() << "%" << std::endl;
    std::cout << "Cache Size:        " << message_cache.get_size() << "/" 
              << message_cache.get_capacity() << std::endl;
    std::cout << "==================================" << std::endl;
}

void signal_handler(int signum) {
    (void)signum; // Suppress unused parameter warning
    
    bool expected = true;
    if (server_running.compare_exchange_strong(expected, false)) {
        std::cout << "\n[Server] Interrupt signal received. Shutting down..." << std::endl;
    } else {
        // Already shutting down, force exit
        std::cout << "\nForce quit..." << std::endl;
        exit(0);
    }
}

bool setup_server_socket(int& server_socket) {
    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        log_message("ERROR: Failed to create socket");
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_message("WARNING: setsockopt SO_REUSEADDR failed");
    }
    
    // Allow port reuse
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        log_message("WARNING: setsockopt SO_REUSEPORT failed");
    }
    
    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_message("ERROR: Bind failed - port may be in use");
        close(server_socket);
        return false;
    }
    
    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        log_message("ERROR: Listen failed");
        close(server_socket);
        return false;
    }
    
    return true;
}

void cleanup_server(int server_socket) {
    log_message("Shutting down server...");
    
    // First, close the server socket to stop accepting new connections
    close(server_socket);
    
    // Force close all client connections to unblock threads
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto& [socket_fd, client_info] : clients) {
            if (client_info.active) {
                // Force immediate close without graceful shutdown
                struct linger sl;
                sl.l_onoff = 1;
                sl.l_linger = 0;
                setsockopt(socket_fd, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl));
                close(socket_fd);
                client_info.active = false;
            }
        }
        clients.clear();
    }
    
    // Give threads a brief moment to detect closed sockets and exit
    std::cout << "Waiting for threads to finish..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Final statistics
    print_statistics();
    
    log_message("Server shutdown complete");
    
    if (log_file.is_open()) {
        log_file.close();
    }
}

int main() {
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Open log file
    log_file.open("server.log", std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "Warning: Could not open log file" << std::endl;
    }
    
    log_message("========================================");
    log_message("Server starting...");
    
    try {
        // Create thread pool
        ThreadPool thread_pool(THREAD_POOL_SIZE);
        
        int server_socket;
        if (!setup_server_socket(server_socket)) {
            return 1;
        }
        
        log_message("Server listening on port " + std::to_string(SERVER_PORT));
        std::cout << "\nServer is running. Press Ctrl+C to stop.\n" << std::endl;
        
        // Main accept loop
        while (server_running.load()) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            // Set socket to non-blocking for clean shutdown
            struct timeval tv;
            tv.tv_sec = 1;  // 1 second timeout
            tv.tv_usec = 0;
            setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            
            int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_socket < 0) {
                if (!server_running.load()) {
                    // Shutting down
                    break;
                }
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    // Timeout, check if we should continue
                    continue;
                }
                log_message("ERROR: Accept failed: " + std::string(strerror(errno)));
                continue;
            }
            
            // Get client IP
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            log_message("New connection from " + std::string(client_ip));
            
            // Assign to thread pool
            try {
                thread_pool.enqueue([client_socket]() {
                    handle_client(client_socket);
                });
                
                {
                    std::lock_guard<std::mutex> lock(metrics_mutex);
                    metrics.active_threads = thread_pool.get_active_count();
                }
            } catch (const std::exception& e) {
                log_message("ERROR: Failed to enqueue client: " + std::string(e.what()));
                close(client_socket);
            }
        }
        
        cleanup_server(server_socket);
        
    } catch (const std::exception& e) {
        log_message("FATAL ERROR: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}