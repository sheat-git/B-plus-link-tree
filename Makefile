bench: bench.cpp tree.cpp
	g++ -Wall -o bench bench.cpp tree.cpp -pthread -std=c++11