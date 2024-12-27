# Variables
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
PYTHON = python3
CPP_SOURCES = $(wildcard src/*.cpp)
CPP_OBJECTS = $(CPP_SOURCES:.cpp=.o)
EXECUTABLE = bin/server.out
PYTHON_SCRIPTS = $(wildcard scripts/*.py)

# Default target
all: build

# Rule to build the C++ executable
build: $(CPP_OBJECTS)
	@echo "Linking C++ objects..."
	$(CXX) $(CXXFLAGS) -o $(EXECUTABLE) $(CPP_OBJECTS)

# Rule to compile C++ source files into objects
%.o: %.cpp
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to clean up build artifacts
clean:
	@echo "Cleaning up..."
	rm -f $(CPP_OBJECTS) $(EXECUTABLE)

# Rule to run Python scripts
run-python:
	@echo "Running Python scripts..."
	@for script in $(PYTHON_SCRIPTS); do \
		echo "Running $$script"; \
		$(PYTHON) $$script; \
	done

# Rule to test Python code
test-python:
	@echo "Testing Python scripts..."
	$(PYTHON) -m unittest discover -s tests -p "*.py"

# Phony targets
.PHONY: all build clean run-python test-python
