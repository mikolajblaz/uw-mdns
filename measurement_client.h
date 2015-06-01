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
  MeasurementClient(boost::asio::io_service& io_service, servers_ptr const& servers) :
      timer(io_service, boost::posix_time::seconds(0)),
      udp_socket(io_service), tcp_socket(io_service), icmp_socket(io_service),
      servers(servers) {
    udp_socket.open(udp::v4());
    // TODO: TCP, ICMP?
    start_measurements(boost::system::error_code());
  }


private:
  boost::asio::deadline_timer timer;

  udp::socket  udp_socket;
  tcp::socket  tcp_socket;
  icmp::socket icmp_socket;

  servers_ptr servers;

  void start_measurements(const boost::system::error_code& error) {
    if (error)
      throw boost::system::system_error(error);

    std::cout << "Measure UDP!\n";

    // TODO może jeden obiekt czas?
    timer.expires_at(timer.expires_at() + boost::posix_time::seconds(MEASUREMENT_INTERVAL_DEFAULT));
    timer.async_wait(boost::bind(&MeasurementClient::start_measurements, this,
        boost::asio::placeholders::error)); // TODO errors
    // TODO czy to działa?
  }
    
  

};

#endif  // MEASURMENT_CLIENT_H