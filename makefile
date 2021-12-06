CC=gcc
INC_DIR=./hbi
CFLAGS=-I$(INC_DIR)
DEPS = hbi.h
SRC_DIR=./read_write_example
SRC_DIR1=./load_firmware_example
SRC_DIR2=./load_grammar_example

OBJ = $(SRC_DIR)/read_write_example.o $(INC_DIR)/hbi.o 

OBJ1 = $(SRC_DIR1)/load_firmware_example.o $(SRC_DIR1)/config.o $(SRC_DIR1)/fwr.o $(INC_DIR)/hbi.o 

OBJ2 = $(SRC_DIR2)/load_grammar_example.o $(SRC_DIR2)/grammar.o $(INC_DIR)/hbi.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

rd_wr_test: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

hbi_load_firmware: $(OBJ1)
	$(CC) -o $@ $^ $(CFLAGS)

hbi_load_grammar: $(OBJ2)
	$(CC) -o $@ $^ $(CFLAGS)
	
clean all:
	rm -f rd_wr_test *.out $(SRC_DIR)/*.o $(INC_DIR)/*.o
	rm -f hbi_load_firmware *.out $(SRC_DIR1)/*.o $(INC_DIR)/*.o
	rm -f hbi_load_grammar *.out $(SRC_DIR2)/*.o $(INC_DIR)/*.o

