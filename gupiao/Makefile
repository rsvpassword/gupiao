CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
TARGET = trading_agent
SRC = src/main.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET) < data/sample_input.txt