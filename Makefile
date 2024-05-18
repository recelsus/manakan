# Compiler settings
CXX = clang++

UNAME_S := $(shell uname -s)

# Set platform-specific flags
ifeq ($(UNAME_S), Linux)
    CXXFLAGS = -Iinclude -DCPPHTTPLIB_OPENSSL_SUPPORT -I/usr/include
    LDFLAGS = -L/usr/lib -lssl -lcrypto -lpthread
endif
ifeq ($(UNAME_S), Darwin)
    CXXFLAGS = -Iinclude -DCPPHTTPLIB_OPENSSL_SUPPORT -I/opt/homebrew/opt/openssl/include
    LDFLAGS = -L/opt/homebrew/opt/openssl/lib -lssl -lcrypto -lpthread
endif

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Build target
TARGET = $(BIN_DIR)/manakan

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Default target
all: $(TARGET)

# Rule to link the program
$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Rule to compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean the build directory
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

