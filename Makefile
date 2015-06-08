CC	= g++
CFLAGS	= -std=c++0x -Wall -Wno-sign-compare -g -I $(BOOST_DIR)
LFLAGS	= -Wall

BOOST_DIR = /home/mikib/lib/C++/boost_1_58_0
LFLAGS_APP = -lboost_system -lpthread

HEADERS = measurement_server.h measurement_client.h server.h print_server.h mdns_server.h \
          mdns_client.h mdns_message.h telnet_server.h telnet_connection.h common.h
TARGET = opoznienia

all: $(TARGET)

$(TARGET).o : %.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET) : % : %.o
	$(CC) $(LFLAGS) -o $@ $^ $(LFLAGS_APP)

.PHONY: clean all
clean:
	rm -f $(TARGET) *.o *~ *.bak
