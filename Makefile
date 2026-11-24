CXXFLAGS ?= -std=c++20 -Wall -Wextra -O2 -g

SRCS := main.cpp cpu.cpp \
        cpu_branch.cpp cpu_sys.cpp cpu_idx.cpp cpu_data.cpp \
        cpu_flag.cpp cpu_stack.cpp cpu_alu.cpp cpu_dump.cpp
OBJS := $(SRCS:.cpp=.o)

tama: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp cpu.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f tama $(OBJS)

.PHONY: clean
