# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -O3 -pthread

# Installation directory
INSTALL_DIR = $(HOME)/.local/bin

# Core binaries to build
all: fizz-daemon fizz-search

# 1. Compile background file tracking daemon
fizz-daemon: services/FileCacheBuilder.cpp services/FileSearch.cpp services/InvertedSearchIndex.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

# 2. Compile light, multi-threaded terminal search application
fizz-search: main.cpp services/QueryOps.cpp services/LoadFile.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

# Installation rule to deploy binaries to your path
install: all
	mkdir -p $(INSTALL_DIR)
	cp fizz-daemon $(INSTALL_DIR)/
	cp fizz-search $(INSTALL_DIR)/

# Clean workspace build fragments cleanly
clean:
	rm -f fizz-daemon fizz-search

.PHONY: all install clean

