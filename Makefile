CXXFLAGS ?= -std=c++20 -Wall -Wextra -O2 -g
CXXFLAGS += $(shell pkg-config --cflags sdl2)
LDLIBS   += $(shell pkg-config --libs sdl2)

SRC_DIR   := src
BUILD_DIR := build

SRCS := main.cpp cpu.cpp \
        cpu_branch.cpp cpu_sys.cpp cpu_idx.cpp cpu_data.cpp \
        cpu_flag.cpp cpu_stack.cpp cpu_alu.cpp cpu_lcd.cpp \
        display.cpp monitor.cpp
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
HDRS := $(wildcard $(SRC_DIR)/*.hpp)

tama: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(HDRS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf tama $(BUILD_DIR)

.PHONY: clean
