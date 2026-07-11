CXX := g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Isrc

SOURCES = src/main.cpp src/glove_parser.cpp src/serial_reader.cpp src/feature_window.cpp
TEST_SOURCES = tests/parser_tests.cpp src/glove_parser.cpp src/serial_reader.cpp
FEATURE_TEST_SOURCES = tests/feature_window_tests.cpp src/feature_window.cpp

.PHONY: all test clean

all: surgymonitor

surgymonitor: $(SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $@

parser_tests: $(TEST_SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TEST_SOURCES) -o $@

feature_window_tests: $(FEATURE_TEST_SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(FEATURE_TEST_SOURCES) -o $@

test: parser_tests feature_window_tests
	./parser_tests
	./feature_window_tests

clean:
	rm -f surgymonitor parser_tests feature_window_tests
