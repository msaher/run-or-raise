CXX = x86_64-w64-mingw32-g++
CXXFLAGS = -Wall -I ./src

.PHONY: build
build: out/run-or-raise.exe
	cp out/run-or-raise.exe ~/desk/run-or-raise/run-or-raise.exe
	cp out/WindowsSucks.dll ~/desk/run-or-raise/WindowsSucks.dll

out/WindowsSucks.dll: src/WindowsSucks.cpp src/WindowsSucks.h
	$(CXX) $(CXXFLAGS) -static-libgcc -static-libstdc++ -shared -o out/WindowsSucks.dll src/WindowsSucks.cpp -Wl,--out-implib,out/libWindowsSucks.a

out/run-or-raise.exe: src/main.cpp out/WindowsSucks.dll
	$(CXX) $(CXXFLAGS) -static-libgcc -static-libstdc++ src/main.cpp -o out/run-or-raise.exe # -Lout -lWindowsSucks -Wl,-allow-multiple-definition


.PHONY: clean
clean:
	rm -f out/*

