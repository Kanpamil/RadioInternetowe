# Variables
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
PYTHON = python3
SRC_DIR = src
OBJ_DIR = bin
CPP_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
CPP_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(CPP_SOURCES))
EXECUTABLE = $(OBJ_DIR)/server.out
PYTHON_SCRIPTS = $(wildcard scripts/*.py)

# Default target
all: $(EXECUTABLE)

# Rule to build the C++ executable
$(EXECUTABLE): $(CPP_OBJECTS)
	@echo "Linking C++ objects..."
	$(CXX) $(CXXFLAGS) -o $@ $(CPP_OBJECTS) -ltag

# Rule to compile C++ source files into objects in the bin directory
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to clean up build artifacts
clean:
	@echo "Cleaning up..."
	rm -rf $(OBJ_DIR)/*

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
.PHONY: all clean run-python test-python
