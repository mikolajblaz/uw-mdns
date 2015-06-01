CC	= g++
CFLAGS	= -std=c++11 -Wall -O2 -I $(BOOST_DIR)
LFLAGS	= -std=c++11 -Wall

BOOST_DIR = /home/mikib/lib/C++/boost_1_58_0
BOOST_FLAG = -lboost_system

HEADERS = measurement_server.h measurement_client.h
TARGET = opoznienia

all: $(TARGET)

$(TARGET).o : %.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET) : % : %.o
	$(CC) $(LFLAGS) -o $@ $^ $(BOOST_FLAG)

.PHONY: clean all
clean:
	rm -f $(TARGET) *.o *~ *.bak
