CXXFLAGS ?= -std=c++20 -Wall -Wextra -O2 -g

SRC_DIR   := src
BUILD_DIR := build

SRCS := main.cpp cpu.cpp \
        cpu_branch.cpp cpu_sys.cpp cpu_idx.cpp cpu_data.cpp \
        cpu_flag.cpp cpu_stack.cpp cpu_alu.cpp cpu_dump.cpp
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)

tama: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(SRC_DIR)/cpu.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf tama $(BUILD_DIR)

.PHONY: clean
