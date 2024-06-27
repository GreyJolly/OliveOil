CC = gcc
EXEC_FILE = test
CFLAGS = -g
OBJECTS = testing_suite.o file_system.o

all: $(EXEC_FILE)

$(EXEC_FILE): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ 

testing_suite.o: testing_suite.c file_system.h
	$(CC) $(CFLAGS) -c -o $@ testing_suite.c
	
file_system.o: file_system.c file_system.h
	$(CC) $(CFLAGS) -c -o $@ file_system.c

clean:
	rm -f $(EXEC_FILE) $(OBJECTS)