CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -g -lpthread -no-pie

# Directories
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = build/obj
BIN_DIR = build/bin

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/main

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Build object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean up
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# all: server.o client.o
# 	echo "compiling server, client"

# server.o: server.cpp conn.h state_enum.h print_utils.h constants.h hashtable.h
# 	echo "compiling server.cpp"
# 	g++ $(CFLAGS) -c hashtable.cpp
# 	g++ $(CFLAGS) -c server.cpp
# 	g++ $(CFLAGS) -o server server.cpp hashtable.cpp

# client.o: client.cpp conn.h state_enum.h print_utils.h constants.h
# 	echo "compiling client.cpp"
# 	g++ $(CFLAGS) client.cpp -o client