# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -g -Iinclude

# Directories
SRC_DIR = src
UTILS_DIR = utils
BUILD_DIR = build
BIN_DIR = bin
INCLUDE_DIR = include

# Find source files
MAIN_SRC = $(wildcard $(SRC_DIR)/*.cc)
UTILS_SRC = $(wildcard $(UTILS_DIR)/*.cc)
ALL_SRC = $(MAIN_SRC) $(UTILS_SRC)

# Generate object file names
MAIN_OBJ = $(MAIN_SRC:$(SRC_DIR)/%.cc=$(BUILD_DIR)/%.o)
UTILS_OBJ = $(UTILS_SRC:$(UTILS_DIR)/%.cc=$(BUILD_DIR)/%.o)
ALL_OBJ = $(MAIN_OBJ) $(UTILS_OBJ)

# Generate binary names (one for each .cc file in src/)
BINARIES = $(MAIN_SRC:$(SRC_DIR)/%.cc=$(BIN_DIR)/%)

# Default target
.PHONY: all clean

all: $(BUILD_DIR) $(BIN_DIR) $(BINARIES)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Create bin directory
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Rule to build binaries (each main .cc file becomes a binary)
$(BIN_DIR)/%: $(SRC_DIR)/%.cc $(UTILS_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $< $(UTILS_OBJ)

# Rule to build object files from src/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Rule to build object files from utils/
$(BUILD_DIR)/%.o: $(UTILS_DIR)/%.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean up generated files
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)

# Show help
help:
	@echo "Available targets:"
	@echo "  all      - Build all binaries"
	@echo "  clean    - Remove build directory and binaries"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Detected source files:"
	@echo "  Main: $(MAIN_SRC)"
	@echo "  Utils: $(UTILS_SRC)"
	@echo ""
	@echo "Will create binaries: $(BINARIES)"

# Dependency tracking (optional but recommended)
-include $(ALL_OBJ:.o=.d)

# Generate dependency files
$(BUILD_DIR)/%.d: $(SRC_DIR)/%.cc | $(BUILD_DIR)
	@$(CXX) $(CXXFLAGS) -MM -MT $(@:.d=.o) $< > $@

$(BUILD_DIR)/%.d: $(UTILS_DIR)/%.cc | $(BUILD_DIR)
	@$(CXX) $(CXXFLAGS) -MM -MT $(@:.d=.o) $< > $@
