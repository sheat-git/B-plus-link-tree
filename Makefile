CXX := /opt/homebrew/bin/g++-12
CXXFLAGS := -Wall -Wextra -pthread -Ofast

bench: bench.cpp tree.cpp

clean:
	$(RM) bench
