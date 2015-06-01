#ifndef COMMON_H
#define COMMON_H

#include <map>
#include <boost/asio/ip/address.hpp>

//uint64_t get_time_usec();

enum PROTOCOL { // TODO nedd it?
	UDP,
	TCP,
	ICMP
};

class Server;
typedef std::map<boost::asio::ip::address, Server > servers_map;
typedef std::shared_ptr<servers_map> servers_ptr;

typedef uint64_t time_type;

const int PROTOCOL_COUNT = 3;
const int BUFFER_SIZE = 100;

const int AVERAGED_MEASUREMENTS = 10;
//const int MAX_DELAY
const int MAX_DELAYED_QUERIES = 10;

/* default values: */
const int TTL_DEFAULT = 10;

const int UDP_PORT_DEFAULT = 3382;
const int UI_PORT_DEFAULT = 3673;
const int MDNS_PORT_DEFAULT = 5353;
const int SSH_PORT_DEFAULT = 22;

const int MEASUREMENT_INTERVAL_DEFAULT = 1;
const int MDNS_INTERVAL_DEFAULT = 10;
const float UI_REFRESH_INTERVAL_DEFAULT = 1.0;
const bool BROADCAST_SSH_DEAFULT = false;



#endif	//COMMON_H