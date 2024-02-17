CC = gcc
CXX = g++

# Google Test requires C++17 and above
CXXFALGS = -Wall -Werror -Wextra -pedantic -std=c++17
LXXFLAGS = -lgtest -lpthread -lpcre2-8 -lm
DEFINES = -DUSE_PCRE_REGEX
SRC=str_test.cpp

all: str trie

str: $(SRC)
	$(CXX) $(DEFINES) $(CXXFALGS) $(SRC) -o str_test $(LXXFLAGS)
	./str_test

trie: trie_test.cpp trie.c
	$(CXX) $(DEFINES) $(CXXFALGS) trie_test.cpp trie.c -o trie_test $(LXXFLAGS)
	./trie_test
	
clean:
	rm -f str_test trie_test