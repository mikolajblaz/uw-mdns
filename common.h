#ifndef COMMON_H
#define COMMON_H

#include <map>
#include <boost/asio/ip/address.hpp>

/* ################## typedefs #################### */

const int PROTOCOL_COUNT = 3;
enum PROTOCOL {
  UDP, TCP, ICMP
};

class Server;
typedef std::map<boost::asio::ip::address, Server> servers_map;   // główna mapa serwerów
typedef std::shared_ptr<servers_map> servers_ptr;

typedef uint64_t time_type;


/* ################## constants #################### */

const long SEC_TO_USEC = 1000000L;    // zamiana sekund na mikrosekundy
const int BUFFER_SIZE = 512;
const int UI_SCREEN_WIDTH = 80;
const int UI_SCREEN_HEIGHT = 24;
const int IP_WIDTH = 15;

const int AVERAGED_MEASUREMENTS = 10; // liczba uśrednianych pomiarów
const int MAX_DELAYED_QUERIES = 10;
const int MAX_DELAY_TIME = 10;        // maksymalne opóźnienie w sekundach
const int TTL_DEFAULT = 20;           // TTL w sekundach

const int SSH_PORT = 22;
const int MDNS_PORT = 5353;
const std::string MDNS_ADDRESS = "224.0.0.251";

const std::string OPOZNIENIA_SERVICE = "_opoznienia._udp.local.";
const std::string SSH_SERVICE = "_ssh._tcp.local.";

const std::string ICMP_MESSAGE = "34686203";


/* ################## default argument values #################### */

const int UDP_PORT_DEFAULT = 3382;
const int UI_PORT_DEFAULT = 3673;

const int MEASUREMENT_INTERVAL_DEFAULT = 1;
const int MDNS_INTERVAL_DEFAULT = 10;
const float UI_REFRESH_INTERVAL_DEFAULT = 1.0;
const bool BROADCAST_SSH_DEFAULT = false;



#endif  //COMMON_H
