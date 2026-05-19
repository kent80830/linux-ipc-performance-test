CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lm

TARGET = perf_test
OBJS = perf_test.o ipc_common.o fifo_impl.o msgq_impl.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) fifo_rd fifo_wr

run: $(TARGET)
	sudo ./$(TARGET)

.PHONY: all clean run
