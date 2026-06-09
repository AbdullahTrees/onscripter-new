.PHONY: all clean

CC = g++
CFLAGS = -Wall -O3 -s -static-libgcc -static-libstdc++ -std=c++23
TARGET = nscdec
OBJS = nscdec.o

all: $(TARGET)

clean:
	rm -f *.o $(TARGET)
	
$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) -o $(TARGET) -lz
