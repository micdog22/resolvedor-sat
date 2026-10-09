# Resolvedor SAT
#   make            compila build/sat
#   make test       testes de unidade e de linha de comando
#   make asan       testes com AddressSanitizer e UndefinedBehaviorSanitizer
#   make clean      apaga a pasta build/

CXXFLAGS ?= -O2
LDFLAGS ?=
BUILD ?= build

WARNINGS = -std=c++17 -Wall -Wextra -Wpedantic

LIB_SRC = $(filter-out src/main.cpp,$(wildcard src/*.cpp))
LIB_OBJ = $(LIB_SRC:src/%.cpp=$(BUILD)/obj/%.o)
HEADERS = $(wildcard src/*.h)

all: $(BUILD)/sat

$(BUILD)/obj:
	mkdir -p $@

$(BUILD)/obj/%.o: src/%.cpp $(HEADERS) | $(BUILD)/obj
	$(CXX) $(WARNINGS) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/sat: $(LIB_OBJ) $(BUILD)/obj/main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD)/testes: tests/testes.cpp tests/teste.h $(LIB_OBJ)
	$(CXX) $(WARNINGS) $(CXXFLAGS) -Isrc $(LDFLAGS) -o $@ tests/testes.cpp $(LIB_OBJ)

test: $(BUILD)/sat $(BUILD)/testes
	./$(BUILD)/testes exemplos
	sh tests/cli.sh ./$(BUILD)/sat exemplos

asan:
	$(MAKE) BUILD=build/asan CXXFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" test

clean:
	rm -rf build

.PHONY: all test asan clean
