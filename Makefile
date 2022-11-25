CXX := /opt/homebrew/bin/g++-12
CXXFLAGS := -Wall -pthread -std=c++11 -g

bench: bench.cpp tree.cpp

clean:
	$(RM) bench
