test: test.cpp tree.cpp
	g++ -Wall -o test test.cpp tree.cpp -pthread -std=c++11