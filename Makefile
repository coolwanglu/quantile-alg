all: test_random test_gk

test_random: test_random.cpp random.h
	g++ -Wall -O3 --std=c++0x -DNDEBUG -o test_random -lboost_timer -lboost_system test_random.cpp

test_gk: test_gk.cpp gk.h
	g++ -Wall -O3 --std=c++0x -DNDEBUG -o test_gk -lboost_timer -lboost_system test_gk.cpp


