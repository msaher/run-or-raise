CXX = x86_64-w64-mingw32-g++
CXXFLAGS = -Wall -Wextra  -static-libgcc -static-libstdc++ -I ./src
RELEASE_FLAGS = -O3

# Default to debug mode
build_mode ?= debug

ifeq ($(build_mode), release)
    CXXFLAGS += $(RELEASE_FLAGS)
endif

out/run-or-raise.exe: src/main.cpp
	$(CXX) $(CXXFLAGS) "$<" -o "$@"

.PHONY: clean
clean:
	rm -f out/*

