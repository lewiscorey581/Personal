# Makefile for Chat Server Project

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -pthread -O2
CXXFLAGS_DEBUG = -std=c++17 -Wall -Wextra -Wpedantic -pthread -g -O0 -DDEBUG
LDFLAGS = -pthread

# Source files
SERVER_SOURCES = server.cpp thread_pool.cpp cache.cpp scheduler.cpp
CLIENT_SOURCES = client.cpp
CACHE_TEST_SOURCES = cache_test.cpp cache.cpp

# Object files
SERVER_OBJECTS = $(SERVER_SOURCES:.cpp=.o)
CLIENT_OBJECTS = $(CLIENT_SOURCES:.cpp=.o)
CACHE_TEST_OBJECTS = $(CACHE_TEST_SOURCES:.cpp=.o)
SERVER_OBJECTS_DEBUG = $(SERVER_SOURCES:.cpp=_debug.o)
CLIENT_OBJECTS_DEBUG = $(CLIENT_SOURCES:.cpp=_debug.o)
CACHE_TEST_OBJECTS_DEBUG = $(CACHE_TEST_SOURCES:.cpp=_debug.o)

# Executables
SERVER_EXEC = server
CLIENT_EXEC = client
CACHE_TEST_EXEC = cache_test
SERVER_EXEC_DEBUG = server_debug
CLIENT_EXEC_DEBUG = client_debug
CACHE_TEST_EXEC_DEBUG = cache_test_debug

# Default target
.DEFAULT_GOAL := all

# Build all targets
all: $(SERVER_EXEC) $(CLIENT_EXEC) $(CACHE_TEST_EXEC)

# Debug builds
debug: $(SERVER_EXEC_DEBUG) $(CLIENT_EXEC_DEBUG) $(CACHE_TEST_EXEC_DEBUG)

# Build server (release)
$(SERVER_EXEC): $(SERVER_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Server built successfully!"

# Build client (release)
$(CLIENT_EXEC): $(CLIENT_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Client built successfully!"

# Build server (debug)
$(SERVER_EXEC_DEBUG): $(SERVER_OBJECTS_DEBUG)
	$(CXX) $(CXXFLAGS_DEBUG) -o $@ $^ $(LDFLAGS)
	@echo "✓ Server debug build completed!"

# Build client (debug)
$(CLIENT_EXEC_DEBUG): $(CLIENT_OBJECTS_DEBUG)
	$(CXX) $(CXXFLAGS_DEBUG) -o $@ $^ $(LDFLAGS)
	@echo "✓ Client debug build completed!"

# Build cache_test (release)
$(CACHE_TEST_EXEC): $(CACHE_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Cache test built successfully!"

# Build cache_test (debug)
$(CACHE_TEST_EXEC_DEBUG): $(CACHE_TEST_OBJECTS_DEBUG)
	$(CXX) $(CXXFLAGS_DEBUG) -o $@ $^ $(LDFLAGS)
	@echo "✓ Cache test debug build completed!"

# Compile source files to object files (release)
%.o: %.cpp common.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile source files to object files (debug)
%_debug.o: %.cpp common.h
	$(CXX) $(CXXFLAGS_DEBUG) -c $< -o $@

# Individual component builds
server: $(SERVER_EXEC)

client: $(CLIENT_EXEC)

cache-test: $(CACHE_TEST_EXEC)

# Clean build artifacts
clean:
	rm -f $(SERVER_OBJECTS) $(CLIENT_OBJECTS) $(CACHE_TEST_OBJECTS)
	rm -f $(SERVER_OBJECTS_DEBUG) $(CLIENT_OBJECTS_DEBUG) $(CACHE_TEST_OBJECTS_DEBUG)
	rm -f $(SERVER_EXEC) $(CLIENT_EXEC) $(CACHE_TEST_EXEC)
	rm -f $(SERVER_EXEC_DEBUG) $(CLIENT_EXEC_DEBUG) $(CACHE_TEST_EXEC_DEBUG)
	rm -f *.log
	@echo "✓ Cleaned all build artifacts and log files"

# Clean only executables
cleanexec:
	rm -f $(SERVER_EXEC) $(CLIENT_EXEC) $(CACHE_TEST_EXEC)
	rm -f $(SERVER_EXEC_DEBUG) $(CLIENT_EXEC_DEBUG) $(CACHE_TEST_EXEC_DEBUG)
	@echo "✓ Removed executables"

# Clean only object files
cleanobj:
	rm -f $(SERVER_OBJECTS) $(CLIENT_OBJECTS) $(CACHE_TEST_OBJECTS)
	rm -f $(SERVER_OBJECTS_DEBUG) $(CLIENT_OBJECTS_DEBUG) $(CACHE_TEST_OBJECTS_DEBUG)
	@echo "✓ Removed object files"

# Clean only logs
cleanlogs:
	rm -f *.log
	@echo "✓ Removed log files"

# Rebuild everything
rebuild: clean all

# Run server
run-server: $(SERVER_EXEC)
	./$(SERVER_EXEC)

# Run server in debug mode
run-server-debug: $(SERVER_EXEC_DEBUG)
	./$(SERVER_EXEC_DEBUG)

# Run client (requires username as argument)
run-client: $(CLIENT_EXEC)
	@if [ -z "$(USER)" ]; then \
		echo "Usage: make run-client USER=<username>"; \
		echo "Example: make run-client USER=alice"; \
		exit 1; \
	fi
	./$(CLIENT_EXEC) $(USER)

# Run client in debug mode
run-client-debug: $(CLIENT_EXEC_DEBUG)
	@if [ -z "$(USER)" ]; then \
		echo "Usage: make run-client-debug USER=<username>"; \
		exit 1; \
	fi
	./$(CLIENT_EXEC_DEBUG) $(USER)

# Run cache test
run-cache-test: $(CACHE_TEST_EXEC)
	./$(CACHE_TEST_EXEC)

# Run multiple clients for testing
test-clients: $(CLIENT_EXEC)
	@echo "Starting 3 test clients..."
	@gnome-terminal -- bash -c "./$(CLIENT_EXEC) Alice; exec bash" 2>/dev/null || \
	 xterm -e "./$(CLIENT_EXEC) Alice" 2>/dev/null || \
	 echo "Terminal emulator not found. Run manually: ./$(CLIENT_EXEC) <username>"
	@sleep 1
	@gnome-terminal -- bash -c "./$(CLIENT_EXEC) Bob; exec bash" 2>/dev/null || \
	 xterm -e "./$(CLIENT_EXEC) Bob" 2>/dev/null || true
	@sleep 1
	@gnome-terminal -- bash -c "./$(CLIENT_EXEC) Charlie; exec bash" 2>/dev/null || \
	 xterm -e "./$(CLIENT_EXEC) Charlie" 2>/dev/null || true

# Check code style (requires clang-format)
format:
	@command -v clang-format >/dev/null 2>&1 || { echo "clang-format not found"; exit 1; }
	clang-format -i *.cpp *.h
	@echo "✓ Code formatted"

# Static analysis (requires cppcheck)
check:
	@command -v cppcheck >/dev/null 2>&1 || { echo "cppcheck not found, skipping..."; exit 0; }
	cppcheck --enable=all --suppress=missingIncludeSystem *.cpp *.h

# Display help
help:
	@echo "Available targets:"
	@echo "  all              - Build server, client, and cache test (default)"
	@echo "  debug            - Build debug versions with symbols"
	@echo "  server           - Build only the server"
	@echo "  client           - Build only the client"
	@echo "  cache-test       - Build only the cache test program"
	@echo "  rebuild          - Clean and rebuild everything"
	@echo ""
	@echo "Running:"
	@echo "  run-server       - Build and run the server"
	@echo "  run-server-debug - Build and run server in debug mode"
	@echo "  run-client       - Build and run client (use: make run-client USER=username)"
	@echo "  run-client-debug - Run client in debug mode"
	@echo "  run-cache-test   - Build and run cache test program"
	@echo "  test-clients     - Launch 3 test clients in separate terminals"
	@echo ""
	@echo "Cleaning:"
	@echo "  clean            - Remove all build artifacts and logs"
	@echo "  cleanexec        - Remove only executables"
	@echo "  cleanobj         - Remove only object files"
	@echo "  cleanlogs        - Remove only log files"
	@echo ""
	@echo "Development:"
	@echo "  format           - Format code with clang-format"
	@echo "  check            - Run static analysis with cppcheck"
	@echo "  help             - Show this help message"

# Phony targets
.PHONY: all debug server client cache-test clean cleanexec cleanobj cleanlogs rebuild \
        run-server run-server-debug run-client run-client-debug run-cache-test test-clients \
        format check help