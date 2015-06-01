#ifndef COMMON_H
#define COMMON_H

#include <map>
#include <boost/asio/ip/address.hpp>

class MeasurementServer;

enum class PROTOCOL {
	UDP,
	TCP,
	ICMP
};

typedef std::map<boost::asio::ip::address, MeasurementServer> servers_map;

const int PROTOCOL_COUNT = 3;

const int AVERAGED_MEASUREMENTS = 10;
//const int MAX_DELAY
const int MAX_DELAYED_QUERIES = 10;

/* default values: */
const int TTL_DEFAULT = 10;

const int SSH_PORT_DEFAULT = 22;
const int UDP_PORT_DEFAULT = 3382;
const int UI_PORT_DEFAULT = 3673;

const int MEASUREMENT_INTERVAL_DEFAULT = 1;
const int DNS_SD_INTERVAL_DEFAULT = 10;
const float UI_REFRESH_INTERVAL_DEFAULT = 1.0;
const bool BROADCAST_SSH_DEAFULT = false;



#endif	//COMMON_H