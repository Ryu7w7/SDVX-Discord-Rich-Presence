CXX = g++
CXXFLAGS = -O2 -Wall -shared -static-libgcc -static-libstdc++
LDFLAGS = -Wl,--kill-at -lurlmon

TARGET = sdvxrpc.dll
SOURCES = main.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET)
