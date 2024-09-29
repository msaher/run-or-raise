CXX = x86_64-w64-mingw32-g++
CXXFLAGS = -Wall -I ./src

.PHONY: build
build: out/run-or-raise.exe
	cp out/run-or-raise.exe ~/desk/run-or-raise/run-or-raise.exe

out/run-or-raise.exe: src/main.cpp
	$(CXX) $(CXXFLAGS) -static-libgcc -static-libstdc++ src/main.cpp -o out/run-or-raise.exe

.PHONY: clean
clean:
	rm -f out/*

