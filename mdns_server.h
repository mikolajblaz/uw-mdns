#ifndef MDNS_SERVER_H
#define MDNS_SERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include "common.h"

using boost::asio::ip::udp;
using boost::asio::ip::address;

class MdnsServer {
public:
  MdnsServer(boost::asio::io_service& io_service) :
      udp_socket(io_service) {
    udp::endpoint listen_endpoint(address::from_string(MDNS_LISTEN_ADDRESS_DEFAULT), MDNS_PORT_DEFAULT_NUM);
    udp_socket.open(listen_endpoint.protocol());
    udp_socket.set_option(udp::socket::reuse_address(true));
    udp_socket.bind(listen_endpoint);

    udp_socket.set_option(boost::asio::ip::multicast::join_group(address::from_string(MDNS_ADDRESS_DEFAULT)));

    start_receive();
  }

private:
  void start_receive() {
    udp_socket.async_receive_from(
        boost::asio::buffer(recv_buffer), remote_endpoint,
        boost::bind(&MdnsServer::handle_receive, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  void handle_receive(const boost::system::error_code& error,
      std::size_t bytes_transferred) {
    if (error)
      throw boost::system::system_error(error);

    std::cout << "mDNS SERVER: datagram received: [";
    std::cout.write(recv_buffer.data(), bytes_transferred);
    std::cout << "]\n";

    boost::shared_ptr<std::string> message(new std::string("DNS Response"));

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
      std::cout << "mDNS SERVER: Sent DNS response!\n" << message;
    }
  }

  boost::array<char, BUFFER_SIZE> recv_buffer;
  udp::socket udp_socket;
  udp::endpoint remote_endpoint;
  udp::endpoint mdns;
};

#endif  // MDNS_SERVER_H