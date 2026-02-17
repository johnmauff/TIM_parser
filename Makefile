# ===============================
# Configuration
# ===============================

CXX       := g++
CXXFLAGS  := -std=c++17 -O2 -Wall -Wextra -pedantic
LDFLAGS   :=

# Path to PEGTL root
PEGTL_DIR := ./external/PEGTL/include

# Target
TARGET    := mom6_parser
SRC       := mom6_parser.cpp

# MOM6 config files in tests directory (adjust pattern as needed)
TEST_DIR  := tests
CONFIG_FILES := $(wildcard $(TEST_DIR)/MOM_input*)

# ===============================
# Build Rules
# ===============================

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -I$(PEGTL_DIR) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

rebuild: clean all

test: $(TARGET)
	@for file in $(CONFIG_FILES); do \
		echo "Processing $$file..."; \
		./$(TARGET) "$$file"; \
	done

.PHONY: all clean rebuild test
