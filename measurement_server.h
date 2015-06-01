#ifndef MEASUREMENT_SERVER_H
#define MEASUREMENT_SERVER_H

#include <boost/asio.hpp>
#include "common.h"

using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using boost::asio::ip::icmp;

class MeasurementServer {
public:



private:
  udp::endpoint  udp_endpoint;
  tcp::endpoint  tcp_endpoint;
  icmp::endpoint icmp_endpoint;

  int finished[PROTOCOL_COUNT][AVERAGED_MEASUREMENTS];
  long waiting[PROTOCOL_COUNT][MAX_DELAYED_QUERIES];

  long delays_sum[PROTOCOL_COUNT];

  // int queries_without_response


  


};

#endif  // MEASURMENT_SERVER_H