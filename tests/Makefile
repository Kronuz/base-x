CXX?=g++
CXXFLAGS=-std=c++14 -Wall -g -I../../googletest/googletest/include -I..
LDFLAGS=-L../../googletest/googlemock/gtest -lgtest -lpthread
TARGET=test

TESTCASES  =
TESTCASES += testcases/tests.o


all: $(TARGET)

.PHONY: clean

$(TESTCASES): %.o : %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): test.cc $(TESTCASES)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -f $(TESTCASES)
