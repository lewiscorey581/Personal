#include "common.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>

std::atomic<bool> client_running(true);

void receive_messages(int socket_fd) {
    Message msg;
    
    while (client_running.load()) {
        memset(&msg, 0, sizeof(msg));
        ssize_t bytes = recv(socket_fd, &msg, sizeof(Message), 0);
        
        if (bytes <= 0) {
            // Connection closed or error
            if (client_running.load()) {
                std::cout << "\n[Server disconnected]" << std::endl;
                client_running.store(false);
            }
            break;
        }
        
        if (bytes != sizeof(Message)) {
            // Partial or invalid message received
            continue;
        }
        
        // Ensure null-terminated strings
        msg.sender[sizeof(msg.sender) - 1] = '\0';
        msg.payload[sizeof(msg.payload) - 1] = '\0';
        
        // Display message based on type
        time_t timestamp = msg.timestamp;
        char time_str[64];
        struct tm* tm_info = localtime(&timestamp);
        if (tm_info) {
            strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
        } else {
            strncpy(time_str, "??:??:??", sizeof(time_str) - 1);
        }
        
        switch (msg.type) {
            case MSG_TEXT:
                std::cout << "\n[" << time_str << "] " << msg.sender << ": " 
                          << msg.payload << std::endl;
                std::cout << "You: " << std::flush;
                break;
                
            case MSG_JOIN:
                std::cout << "\n[" << time_str << "] >>> " << msg.payload << std::endl;
                std::cout << "You: " << std::flush;
                break;
                
            case MSG_LEAVE:
                std::cout << "\n[" << time_str << "] <<< " << msg.payload << std::endl;
                std::cout << "You: " << std::flush;
                break;
                
            default:
                // Unknown message type, ignore
                break;
        }
    }
}

void send_messages(int socket_fd, const std::string& user_id) {
    std::string input;
    Message msg;
    
    while (client_running.load()) {
        std::cout << "You: " << std::flush;
        
        if (!std::getline(std::cin, input)) {
            // EOF or error on stdin
            client_running.store(false);
            break;
        }
        
        if (!client_running.load()) break;
        
        // Check for exit command
        if (input == "/quit" || input == "/exit") {
            client_running.store(false);
            break;
        }
        
        // Check for help command
        if (input == "/help") {
            std::cout << "\nAvailable commands:" << std::endl;
            std::cout << "  /quit, /exit   - Disconnect from chat" << std::endl;
            std::cout << "  /help          - Show this help message" << std::endl;
            std::cout << "  /stats         - Request server statistics" << std::endl;
            std::cout << "  /cachetest N   - Send N messages to test cache (e.g., /cachetest 20)" << std::endl;
            std::cout << std::endl;
            continue;
        }
        
        // Check for stats command
        if (input == "/stats") {
            memset(&msg, 0, sizeof(msg));
            msg.type = MSG_STATUS;
            msg.set_sender(user_id);
            msg.timestamp = time(nullptr);
            
            ssize_t sent = send(socket_fd, &msg, sizeof(Message), MSG_NOSIGNAL);
            if (sent <= 0) {
                std::cout << "\n[ERROR] Failed to send stats request" << std::endl;
                client_running.store(false);
                break;
            }
            std::cout << "Requesting statistics from server..." << std::endl;
            continue;
        }
        
        // Check for cache test command
        if (input.substr(0, 11) == "/cachetest " || input == "/cachetest") {
            int num_messages = 20;  // Default
            
            if (input.length() > 11) {
                try {
                    num_messages = std::stoi(input.substr(11));
                    if (num_messages <= 0 || num_messages > 100) {
                        std::cout << "[ERROR] Number of messages must be between 1 and 100" << std::endl;
                        continue;
                    }
                } catch (const std::exception& e) {
                    std::cout << "[ERROR] Invalid number format. Usage: /cachetest N" << std::endl;
                    continue;
                }
            }
            
            std::cout << "Sending " << num_messages << " test messages to fill cache..." << std::endl;
            
            for (int i = 0; i < num_messages; i++) {
                memset(&msg, 0, sizeof(msg));
                msg.type = MSG_TEXT;
                msg.set_sender(user_id);
                
                std::string test_msg = "Cache test message #" + std::to_string(i + 1);
                msg.set_payload(test_msg);
                msg.timestamp = time(nullptr);
                
                ssize_t sent = send(socket_fd, &msg, sizeof(Message), MSG_NOSIGNAL);
                if (sent <= 0) {
                    std::cout << "\n[ERROR] Failed to send test message " << (i + 1) << std::endl;
                    break;
                }
                
                // Small delay to avoid overwhelming the server
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            std::cout << "Sent " << num_messages << " test messages. Use /stats to see cache statistics." << std::endl;
            continue;
        }
        
        // Skip empty messages
        if (input.empty()) {
            continue;
        }
        
        // Check message length
        if (input.length() >= BUFFER_SIZE) {
            std::cout << "[WARNING] Message too long, truncating to " << (BUFFER_SIZE - 1) 
                      << " characters" << std::endl;
            input = input.substr(0, BUFFER_SIZE - 1);
        }
        
        // Send text message
        memset(&msg, 0, sizeof(msg));
        msg.type = MSG_TEXT;
        msg.set_sender(user_id);
        msg.set_payload(input);
        msg.timestamp = time(nullptr);
        
        ssize_t sent = send(socket_fd, &msg, sizeof(Message), MSG_NOSIGNAL);
        if (sent <= 0) {
            std::cout << "\n[ERROR] Failed to send message" << std::endl;
            client_running.store(false);
            break;
        }
    }
}

bool connect_to_server(const std::string& server_ip, int server_port, 
                       const std::string& user_id, int& client_socket) {
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        std::cerr << "ERROR: Failed to create socket" << std::endl;
        return false;
    }
    
    // Setup server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "ERROR: Invalid server address: " << server_ip << std::endl;
        close(client_socket);
        return false;
    }
    
    // Connect to server with timeout
    std::cout << "Connecting to server at " << server_ip << ":" << server_port << "..." << std::endl;
    
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "ERROR: Connection failed. Is the server running?" << std::endl;
        close(client_socket);
        return false;
    }
    
    std::cout << "Connected to server!" << std::endl;
    
    // Send user ID to server
    ssize_t sent = send(client_socket, user_id.c_str(), user_id.length(), MSG_NOSIGNAL);
    if (sent <= 0) {
        std::cerr << "ERROR: Failed to send user ID to server" << std::endl;
        close(client_socket);
        return false;
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1";
    int server_port = SERVER_PORT;
    std::string user_id;
    
    // Parse command line arguments
    if (argc >= 2) {
        user_id = argv[1];
    } else {
        std::cout << "Enter your username: ";
        if (!std::getline(std::cin, user_id)) {
            std::cerr << "ERROR: Failed to read username" << std::endl;
            return 1;
        }
    }
    
    // Validate username
    if (user_id.empty()) {
        std::cerr << "ERROR: Username cannot be empty" << std::endl;
        return 1;
    }
    
    if (user_id.length() > USERNAME_MAX_LEN) {
        std::cerr << "ERROR: Username too long (max " << USERNAME_MAX_LEN << " characters)" << std::endl;
        return 1;
    }
    
    // Check for invalid characters
    for (char c : user_id) {
        if (!isprint(c) || c == '\n' || c == '\r') {
            std::cerr << "ERROR: Username contains invalid characters" << std::endl;
            return 1;
        }
    }
    
    if (argc >= 3) {
        server_ip = argv[2];
    }
    
    if (argc >= 4) {
        try {
            server_port = std::stoi(argv[3]);
            if (server_port <= 0 || server_port > 65535) {
                std::cerr << "ERROR: Invalid port number (must be 1-65535)" << std::endl;
                return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Invalid port number: " << argv[3] << std::endl;
            return 1;
        }
    }
    
    int client_socket;
    if (!connect_to_server(server_ip, server_port, user_id, client_socket)) {
        return 1;
    }
    
    std::cout << "\nWelcome to the chat, " << user_id << "!" << std::endl;
    std::cout << "Type /help for available commands" << std::endl;
    std::cout << "Type /quit to disconnect\n" << std::endl;
    
    // Start receiver thread
    std::thread receiver_thread(receive_messages, client_socket);
    
    // Main thread handles sending
    send_messages(client_socket, user_id);
    
    // Cleanup
    std::cout << "\nDisconnecting from server..." << std::endl;
    client_running.store(false);
    
    // Shutdown socket to wake up receiver thread
    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
    
    // Wait for receiver thread to finish (with timeout)
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }
    
    std::cout << "Disconnected successfully. Goodbye!" << std::endl;
    
    return 0;
}