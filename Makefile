CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra
SRC = main.cpp database.cpp engine.cpp scenarios.cpp

nrr_engine: $(SRC) tournament.h
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

clean:
	rm -f nrr_engine
