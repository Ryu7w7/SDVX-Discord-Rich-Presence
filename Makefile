CXX = g++
CXXFLAGS = -O3 -Wall -shared -static-libgcc -static-libstdc++
LDFLAGS = -Wl,--kill-at -s

TARGET = sdvxrpc.dll
SOURCES = main.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET)
