CC = gcc
CFLAGS = -Wall -g
TARGET = dragonshell
SRCS = dragonshell.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

run: $(TARGET)
	./$(TARGET)

compile: $(OBJS)
	
clean:

	rm -f $(TARGET)


%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@