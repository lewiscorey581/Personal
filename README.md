# Final Project: Multi-threaded Chat Server with LRU Cache

A high-performance, multi-threaded chat server implementation in C++ featuring thread pooling, message caching with LRU eviction, round-robin scheduling, and comprehensive testing capabilities.

## Features

- **Thread Pool Architecture**: Fixed-size thread pool (6 threads) for efficient concurrent client handling
- **LRU Message Cache**: Thread-safe cache with Least Recently Used eviction policy (capacity: 10 messages)
- **Round-Robin Scheduler**: Fair scheduling of client message processing with circular linked list
- **Robust Error Handling**: Comprehensive error checking and graceful degradation
- **Performance Metrics**: Real-time statistics on messages, cache efficiency, and active connections
- **Testing Tools**: Standalone cache test program and interactive client commands

## Compilation Instructions



### Build Commands

```bash
# Build all components (server, client, cache test)
make all

# Clean all build artifacts
make clean

## Testing Guide

### Test 1: Basic Connectivity (Single Client)
- Verify basic server-client connection and message handling

1. **Start the server**
   ```bash
   # Terminal 1
   ./server

2. **Connect one client**
   ```bash
   # Terminal 2
  ./client Alice


3. **Type a message in the client**
   ```bash
   You: Hello, this is a test message!
   ```

4. **Check that the message is logged on the server**
   
   The terminal display should show: Message from Alice: "Test Message!"


### Test 2: Multi-Client Communication

**Purpose**: Verify message broadcasting to multiple clients

1. **Start the server**
   ```bash
   # Terminal 1
    ./server



2. **Connect 3-5 clients with different usernames**
   ```bash
   # Terminal 2
   ./client Alice
   
   # Terminal 3
   ./client Bob
   
   # Terminal 4
   ./client Charlie
   
   # Terminal 5 (optional)
   ./client Diane
   
   # Terminal 6 (optional)
   ./client Eve

3. **Send messages from different clients**
   ```bash
   # In Alice's terminal:
   You: Hello everyone from Alice!
   
   # In Bob's terminal:
   You: Hi Alice, this is Bob!
   
   # In Charlie's terminal:
   You: Charlie here, I can see your messages!
   ```

4. **Verify all clients receive broadcasted messages**
   
   Each client should see messages from other clients

### Test 3: Client Join/Leave Notifications

**Purpose**: Verify join/leave notifications are broadcasted correctly

1. **Start server with 2 connected clients**
   ```bash
   # Terminal 1: Server
   ./server
   
   # Terminal 2: Alice
   ./client Alice
   
   # Terminal 3: Bob
   ./client Bob

2. **Connect a new client (Client 3)**
   ```bash
   # Terminal 4: Charlie
   ./client Charlie

3. **Verify existing clients see "X has joined the chat"**
   
   Alice and Bob's terminals should show: >>> Charlie has joined the chat

4. **Disconnect a client using `/quit`**
   ```bash
   # In Charlie's terminal:
   /quit

5. **Verify remaining clients see "X has left the chat"**
   
   Alice and Bob's terminals should show: <<< Charlie has left the chat

6. **Disconnecting the server**

    To disconnect the server enter ctrl^C twice, once to begin the server shut down, then again to completly exit to terminal command line.

### Test 4: Thread Pool Management

**Purpose**: Verify thread pool handles multiple concurrent clients efficiently

1. **Start the server**
   ```bash
   ./server
   
   Note the thread pool initialization:
   ```
   [ThreadPool] Created with 6 worker threads
   ```

2. **Rapidly connect 10+ clients**
   Or manually open 10+ terminal windows and connect clients

3. **Observe server logs for thread pool activity**
   
   Server should show several clients joining

4. **Check that all clients are handled by the thread pool**
   
   Send messages from multiple clients simultaneously and verify:
   - No connection refused errors
   - All messages are processed
   - Server remains responsive
   - No deadlocks or hangs

   Check server statistics:
   ```bash
   # From any client:
   You: /stats
   ```

### Test 5: Cache Performance

**Purpose**: Test LRU cache functionality and performance metrics

#### Cache Test

```bash
# Run comprehensive automated cache tests
make run-cache-test
```

### Test 6: Round-Robin Scheduling

**Purpose**: Verify fair scheduling of client message processing

1. **Start server**
   ```bash
   ./server
   ```

2. **Connect multiple clients**
   ```bash
   # Terminal 2-5
   ./client ...
   ./client ...
   etc
   ```

3. **Send messages from different clients**
   ```bash
   # Each client sends a message with their client number
   Client1: Message from Client 1
   Client2: Message from Client 2
   etc
   ```

4. **Observe server logs for scheduling information**

   Messages are processed in round-robin order, ensuring fair scheduling.

```

## Client Commands

| Command | Description | Example |
|---------|-------------|---------|
| `/help` | Show available commands |
| `/stats` | Request server statistics |
| `/quit`| Disconnect from server |

