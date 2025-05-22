CXX = g++
CXXFLAGS = -Wall -std=c++11 -pthread
TARGET_SERVER = echo-server
TARGET_CLIENT = echo-client
SRCS_SERVER = echo-server.cpp
SRCS_CLIENT = echo-client.cpp

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(SRCS_SERVER)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_CLIENT): $(SRCS_CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT)

.PHONY: all clean
