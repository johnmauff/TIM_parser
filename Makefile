# ===============================
# Configuration
# ===============================

CXX       := g++
CXXFLAGS  := -std=c++17 -O2 -Wall -Wextra -pedantic
LDFLAGS   :=

# Path to PEGTL root
PEGTL_DIR := ./PEGTL/include

# Target
TARGET    := mom6_parser
SRC       := mom6_parser.cpp


# ===============================
# Build Rules
# ===============================

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -I$(PEGTL_DIR) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild

