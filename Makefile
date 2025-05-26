# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -O2 -g -Iinclude

# Directories
SRC_DIR = src
UTILS_DIR = utils
BUILD_DIR = build
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
BINARIES = $(MAIN_SRC:$(SRC_DIR)/%.cc=%)

# Default target
.PHONY: all clean

all: $(BUILD_DIR) $(BINARIES)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Rule to build binaries (each main .cc file becomes a binary)
%: $(SRC_DIR)/%.cc $(UTILS_OBJ) | $(BUILD_DIR)
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
	rm -f $(BINARIES)

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
