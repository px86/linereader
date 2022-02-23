CXX = g++
CXXFLAGS = -Wall -Werror -Wpedantic -std=c++11 -Isrc/include
LIBS =

all: test

test: src/test.cpp linereader.o
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: src/%.cpp src/include/%.hpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

keycode: src/tools/keycodes.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf *.o test keycode
