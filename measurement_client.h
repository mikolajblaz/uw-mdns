#ifndef MEASUREMENT_CLIENT_H
#define MEASUREMENT_CLIENT_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common.h"

using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using boost::asio::ip::icmp;

class MeasurementClient {
public:
  MeasurementClient(boost::asio::io_service& io_service, std::shared_ptr<servers_map> const& servers) :
      timer(io_service, boost::posix_time::seconds(0)),
      udp_socket(io_service), tcp_socket(io_service), icmp_socket(io_service),
      servers(servers) {
    udp_socket.open(udp::v4());
    init_measurements();
  }


private:
  boost::asio::deadline_timer timer;

  udp::socket  udp_socket;
  tcp::socket  tcp_socket;
  icmp::socket icmp_socket;

  std::shared_ptr<servers_map> servers;

  void init_measurements() {
    std::cout << "Measure UDP!\n";

    timer.expires_at(timer.expires_at() + boost::posix_time::seconds(MEASUREMENT_INTERVAL_DEFAULT));
    timer.async_wait(boost::bind(&MeasurementClient::init_measurements, this));
  }
    
  

};

#endif  // MEASURMENT_CLIENT_H