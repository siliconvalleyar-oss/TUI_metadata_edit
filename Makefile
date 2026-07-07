CXX      := g++
FTXUI_DIR ?= /tmp/FTXUI
FTXUI_INSTALL ?= /tmp/ftxui-install
CXXFLAGS := -std=c++17 -fPIC -Wall -Wextra -O2 -Iinclude -I$(FTXUI_DIR)/include -I$(FTXUI_INSTALL)/usr/local/include
LDFLAGS  := -L$(FTXUI_INSTALL)/usr/local/lib -lftxui-component -lftxui-dom -lftxui-screen -lstdc++fs

TARGET   := MetadataMP3
SRC      := src
OBJ      := obj
BIN      := bin

SOURCES  := $(wildcard $(SRC)/*.cpp)
OBJECTS  := $(patsubst $(SRC)/%.cpp,$(OBJ)/%.o,$(SOURCES))

# Check FTXUI is installed
FTXUI_CHECK := $(FTXUI_INSTALL)/usr/local/include/ftxui/component/component.hpp

.SECONDARY:
.PHONY: all clean distclean run deps

all: check-deps $(BIN)/$(TARGET)

check-deps:
	@if [ ! -f "$(FTXUI_CHECK)" ]; then \
		echo ""; \
		echo "  FTXUI not found at $(FTXUI_INSTALL)"; \
		echo "  Run: ./script_tools/install_deps.sh"; \
		echo ""; \
		exit 1; \
	fi

deps:
	./script_tools/install_deps.sh

$(OBJ):
	mkdir -p $(OBJ)

$(BIN):
	mkdir -p $(BIN)

$(OBJ)/%.o: $(SRC)/%.cpp | $(OBJ)
	$(CXX) $(CXXFLAGS) -MMD -MF $(@:.o=.d) -c $< -o $@

$(BIN)/$(TARGET): $(OBJECTS) | $(BIN)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

run: $(BIN)/$(TARGET)
	./$(BIN)/$(TARGET)

clean:
	rm -f $(OBJ)/*.o $(OBJ)/*.d $(BIN)/$(TARGET)

distclean: clean
	rm -rf $(OBJ) $(BIN)

-include $(wildcard $(OBJ)/*.d)
