CXX := g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Isrc
LDLIBS ?= -larmadillo
THREAD_FLAGS ?= -pthread

SOURCES = src/main.cpp src/glove_parser.cpp src/serial_reader.cpp src/feature_window.cpp src/anomaly_detector.cpp src/monitor.cpp src/runtime_support.cpp
TEST_SOURCES = tests/parser_tests.cpp src/glove_parser.cpp src/serial_reader.cpp
FEATURE_TEST_SOURCES = tests/feature_window_tests.cpp src/feature_window.cpp
ANOMALY_TEST_SOURCES = tests/anomaly_detector_tests.cpp src/anomaly_detector.cpp
RUNTIME_TEST_SOURCES = tests/runtime_support_tests.cpp src/runtime_support.cpp
MONITOR_RUNTIME_TEST_SOURCES = tests/monitor_runtime_tests.cpp src/glove_parser.cpp src/serial_reader.cpp src/feature_window.cpp src/anomaly_detector.cpp src/monitor.cpp src/runtime_support.cpp

.PHONY: all test test-host test-mlpack test-runtime clean

all: surgymonitor

surgymonitor: $(SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(THREAD_FLAGS) $(SOURCES) -o $@ $(LDLIBS) $(THREAD_FLAGS)

parser_tests: $(TEST_SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TEST_SOURCES) -o $@

feature_window_tests: $(FEATURE_TEST_SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(FEATURE_TEST_SOURCES) -o $@

anomaly_detector_tests: $(ANOMALY_TEST_SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ANOMALY_TEST_SOURCES) -o $@ $(LDLIBS)

runtime_support_tests: $(RUNTIME_TEST_SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(THREAD_FLAGS) $(RUNTIME_TEST_SOURCES) -o $@ $(THREAD_FLAGS)

test-host: parser_tests feature_window_tests runtime_support_tests
	./parser_tests
	./feature_window_tests
	./runtime_support_tests

test-mlpack: anomaly_detector_tests
	./anomaly_detector_tests

monitor_runtime_tests: $(MONITOR_RUNTIME_TEST_SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(THREAD_FLAGS) $(MONITOR_RUNTIME_TEST_SOURCES) -o $@ $(LDLIBS) $(THREAD_FLAGS)

test-runtime: monitor_runtime_tests
	./monitor_runtime_tests

test: test-host test-mlpack test-runtime

clean:
	rm -f surgymonitor parser_tests feature_window_tests anomaly_detector_tests runtime_support_tests monitor_runtime_tests
