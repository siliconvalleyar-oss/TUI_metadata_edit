CXX      := g++
CXXFLAGS := -std=c++17 -fPIC -Wall -Wextra -O2 -Iinclude -I/tmp/FTXUI/include -I/tmp/ftxui-install/usr/local/include
LDFLAGS  := -L/tmp/ftxui-install/usr/local/lib -lftxui-component -lftxui-dom -lftxui-screen -lstdc++fs

TARGET   := MetadataMP3
SRC      := src
OBJ      := obj
BIN      := bin

SOURCES  := $(wildcard $(SRC)/*.cpp)
OBJECTS  := $(patsubst $(SRC)/%.cpp,$(OBJ)/%.o,$(SOURCES))

.SECONDARY:
.PHONY: all clean distclean run

all: $(BIN)/$(TARGET)

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
