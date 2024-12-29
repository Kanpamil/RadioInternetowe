CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra $(shell pkg-config --cflags libavformat libavcodec libavutil libswscale libswresample)
LDFLAGS = $(shell pkg-config --libs libavformat libavcodec libavutil libswscale libswresample)

SOURCES = $(wildcard src/*.cpp)
BIN_DIR = bin
TARGETS = $(SOURCES:src/%.cpp=$(BIN_DIR)/%.out)

all: $(TARGETS)

$(BIN_DIR)/%.out: src/%.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

%.out: %.cpp
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/$@ $< $(LDFLAGS)
clean:
	rm -f $(TARGETS)
	@rmdir --ignore-fail-on-non-empty $(BIN_DIR) 2>/dev/null || true

