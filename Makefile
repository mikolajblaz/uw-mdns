CC	= cc
CFLAGS	= -Wall -O2
LFLAGS	= -Wall

TARGET = opoznienia

all: $(TARGET)

opoznienia: opoznienia.o err.o
	$(CC) $(LFLAGS) $^ -o $@

.PHONY: clean all
clean:
	rm -f $(TARGET) *.o *~ *.bak
