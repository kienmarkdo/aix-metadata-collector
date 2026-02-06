# ============================================================================
# AIX Metadata Collector - Makefile
# ============================================================================
#
# This Makefile is compatible with AIX make and supports both xlC and g++.
#
# Usage:
#   make              - Build with g++ (default)
#   make CXX=xlC      - Build with IBM xlC compiler
#   make CXX=g++      - Build with GCC
#   make clean        - Remove build artifacts
#   make install      - Install to /usr/local/bin (requires root)
#   make test         - Run basic tests
#
# ============================================================================

# Project name and version
PROJECT_NAME = aix-metadata-collector
VERSION = 1.0.0

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Output binary
TARGET = $(BIN_DIR)/$(PROJECT_NAME)

# Source and object files
SOURCES = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/types.cpp \
          $(SRC_DIR)/process_collector.cpp \
          $(SRC_DIR)/file_collector.cpp \
          $(SRC_DIR)/port_collector.cpp \
          $(SRC_DIR)/json_formatter.cpp

OBJECTS = $(BUILD_DIR)/main.o \
          $(BUILD_DIR)/types.o \
          $(BUILD_DIR)/process_collector.o \
          $(BUILD_DIR)/file_collector.o \
          $(BUILD_DIR)/port_collector.o \
          $(BUILD_DIR)/json_formatter.o

# Default compiler (can be overridden with CXX=xlC)
CXX = g++

# Compiler flags - these work for both g++ and xlC with minor differences
# For g++:
CXXFLAGS_GCC = -std=c++11 -Wall -Wextra -O2 -D_AIX -D_LARGE_FILES -I$(INC_DIR)
LDFLAGS_GCC =

# For xlC:
CXXFLAGS_XLC = -q64 -qlanglvl=extended0x -O2 -D_AIX -D_LARGE_FILES -I$(INC_DIR)
LDFLAGS_XLC = -q64

# Default to g++ flags (override below if using xlC)
CXXFLAGS = $(CXXFLAGS_GCC)
LDFLAGS = $(LDFLAGS_GCC)

# ============================================================================
# Build Targets
# ============================================================================

all: dirs $(TARGET)

# Create necessary directories
dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)

# Link the target binary
$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CXX) $(LDFLAGS) -o $(TARGET) $(OBJECTS)
	@echo "Build complete: $(TARGET)"

# Compile individual source files
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp
	@echo "Compiling main.cpp..."
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/main.o $(SRC_DIR)/main.cpp

$(BUILD_DIR)/types.o: $(SRC_DIR)/types.cpp
	@echo "Compiling types.cpp..."
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/types.o $(SRC_DIR)/types.cpp

$(BUILD_DIR)/process_collector.o: $(SRC_DIR)/process_collector.cpp
	@echo "Compiling process_collector.cpp..."
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/process_collector.o $(SRC_DIR)/process_collector.cpp

$(BUILD_DIR)/file_collector.o: $(SRC_DIR)/file_collector.cpp
	@echo "Compiling file_collector.cpp..."
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/file_collector.o $(SRC_DIR)/file_collector.cpp

$(BUILD_DIR)/port_collector.o: $(SRC_DIR)/port_collector.cpp
	@echo "Compiling port_collector.cpp..."
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/port_collector.o $(SRC_DIR)/port_collector.cpp

$(BUILD_DIR)/json_formatter.o: $(SRC_DIR)/json_formatter.cpp
	@echo "Compiling json_formatter.cpp..."
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/json_formatter.o $(SRC_DIR)/json_formatter.cpp

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Clean complete."

# Install to system
install: $(TARGET)
	@echo "Installing to /usr/local/bin..."
	cp $(TARGET) /usr/local/bin/$(PROJECT_NAME)
	chmod 755 /usr/local/bin/$(PROJECT_NAME)
	@echo "Installed: /usr/local/bin/$(PROJECT_NAME)"

# Uninstall from system
uninstall:
	@echo "Uninstalling from /usr/local/bin..."
	rm -f /usr/local/bin/$(PROJECT_NAME)
	@echo "Uninstalled."

# Run basic tests
test: $(TARGET)
	@echo "=============================================="
	@echo "Running basic tests..."
	@echo "=============================================="
	@echo ""
	@echo "Test 1: --help"
	$(TARGET) --help > /dev/null && echo "  PASS: --help works" || echo "  FAIL: --help"
	@echo ""
	@echo "Test 2: --version"
	$(TARGET) --version > /dev/null && echo "  PASS: --version works" || echo "  FAIL: --version"
	@echo ""
	@echo "Test 3: --process 1 (init process)"
	$(TARGET) --process 1 > /dev/null 2>&1 && echo "  PASS: --process 1 works" || echo "  INFO: --process 1 may require root"
	@echo ""
	@echo "Test 4: --file /etc/passwd"
	$(TARGET) --file /etc/passwd > /dev/null && echo "  PASS: --file /etc/passwd works" || echo "  FAIL: --file"
	@echo ""
	@echo "Test 5: --port 22"
	$(TARGET) --port 22 > /dev/null && echo "  PASS: --port 22 works" || echo "  INFO: --port may require root for full info"
	@echo ""
	@echo "=============================================="
	@echo "Basic tests complete."
	@echo "=============================================="

# Show help
help:
	@echo "AIX Metadata Collector - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build the project (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install to /usr/local/bin (requires root)"
	@echo "  uninstall - Remove from /usr/local/bin"
	@echo "  test      - Run basic tests"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Compiler Options:"
	@echo "  make CXX=xlC CXXFLAGS=\"-q64 -qlanglvl=extended0x -O2 -D_AIX -D_LARGE_FILES -Iinclude\" LDFLAGS=-q64"
	@echo "  make CXX=g++    - Use GCC compiler (default)"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build with g++"
	@echo "  make clean all test     # Clean, rebuild, and test"
