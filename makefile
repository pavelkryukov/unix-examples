CC:=gcc
CCFLAGS:= -O3 -Wall -Werror -std=gnu89 -pedantic

CXX:=g++
CXXFLAGS:= -O3 -Wall -Werror -std=c++98 -pedantic

SRC_DIR:=source
BIN_DIR:=bin

C_FILES := \
	$(SRC_DIR)/mycopy.c \
	$(SRC_DIR)/morze.c \
	$(SRC_DIR)/myshell.c

CPP_FILES := \
	$(SRC_DIR)/useless.cpp \
	$(SRC_DIR)/office.cpp  
    
BIN_FILES:=${C_FILES:$(SRC_DIR)/%.c=$(BIN_DIR)/%} \
	${CPP_FILES:$(SRC_DIR)/%.cpp=$(BIN_DIR)/%} 

all: build_dirs $(BIN_FILES)

bin/mycopy: source/mycopy.c
	$(CC) $(CCFLAGS) $< -o $@ 

bin/useless: source/useless.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ 
    
bin/myshell: source/myshell.c
	$(CC) $(CCFLAGS) $< -o $@ 
    
bin/morze: source/morze.c
	$(CC) $(CCFLAGS) $< -o $@ 

bin/office: source/office.cpp
	$(CXX) $(CXXFLAGS) -pthread $< -o $@ 

build_dirs:
	mkdir -p $(BIN_DIR)
    
clean:
	rm -rf $(BIN_DIR)  

.PHONY: clean
