#ifndef MEASUREMENT_SERVER_H
#define MEASUREMENT_SERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <endian.h>
#include "common.h"
#include "get_time_usec.h"

using boost::asio::ip::udp;

class MeasurementServer {
public:
  MeasurementServer(boost::asio::io_service& io_service) :
      socket(io_service, udp::endpoint(udp::v4(), UDP_PORT_DEFAULT)) {
    start_receive();
  }

private:
  void start_receive() {
    socket.async_receive_from(boost::asio::buffer(time_buffer), remote_endpoint,
        boost::bind(&MeasurementServer::handle_receive, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  void handle_receive(const boost::system::error_code& error,
      std::size_t bytes_transferred) {
    if (!error && bytes_transferred >= sizeof(uint64_t)) {   // else ignorujemy

      time_buffer[1] = htobe64(get_time_usec());

      socket.async_send_to(boost::asio::buffer(time_buffer), remote_endpoint,
          boost::bind(&MeasurementServer::handle_send, this));

      start_receive();
    }
  }

  void handle_send() {}

  boost::array<uint64_t, 2> time_buffer;

  udp::socket socket;
  udp::endpoint remote_endpoint;

};

#endif  // MEASUREMENT_SERVER_H