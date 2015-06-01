#ifndef MDNS_SERVER_H
#define MDNS_SERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include "common.h"

using boost::asio::ip::udp;

class MdnsServer {
public:
  MdnsServer(boost::asio::io_service& io_service) :
      udp_socket(io_service, udp::endpoint(udp::v4(), MDNS_PORT_DEFAULT)) {
    start_receive();
  }

private:
  udp::socket udp_socket;
  udp::endpoint remote_endpoint;
  boost::array<char, BUFFER_SIZE> recv_buffer;


  void start_receive() {
    udp_socket.async_receive_from(
        boost::asio::buffer(recv_buffer), remote_endpoint,
        boost::bind(&MdnsServer::handle_receive, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  void handle_receive(const boost::system::error_code& error,
      std::size_t /*bytes_transferred*/) {
    if (error)
      throw boost::system::system_error(error);

    boost::shared_ptr<std::string> message(
        new std::string("DNS Response"));

    udp_socket.async_send_to(boost::asio::buffer(*message), remote_endpoint,
        boost::bind(&MdnsServer::handle_send, this, message,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));

    start_receive();
  }

  void handle_send(boost::shared_ptr<std::string> message,
      const boost::system::error_code& error,
      std::size_t /*bytes_transferred*/) {
    if (error) {
      std::cout << "Error in sending DNS response!\n";
      throw boost::system::system_error(error);
    } else {
      std::cout << "Sent DNS response! " << message;
    }
  }


};

#endif  // MDNS_SERVER_H