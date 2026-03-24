CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude
LDFLAGS  := -lsfml-graphics -lsfml-window -lsfml-system

TARGET   := predprey
SRC_DIR  := src
INC_DIR  := include

# Recursively find all .cpp files under src/
SRCS     := $(shell find $(SRC_DIR) -name '*.cpp')
OBJS     := $(SRCS:.cpp=.o)

# Also find any .cpp files inside include/ subdirectories (if any headers have impl files)
INC_SRCS := $(shell find $(INC_DIR) -name '*.cpp' 2>/dev/null)
INC_OBJS := $(INC_SRCS:.cpp=.o)

ALL_OBJS := $(OBJS) $(INC_OBJS)

# ─── Targets ──────────────────────────────────────────────────────────────────

.PHONY: all clean rebuild

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Compile any .cpp -> .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(ALL_OBJS) $(TARGET)

rebuild: clean all