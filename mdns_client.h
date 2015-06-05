#ifndef MDNS_CLIENT_H
#define MDNS_CLIENT_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common.h"

using boost::asio::ip::udp;
using boost::asio::ip::address;

class MdnsClient {
public:
  MdnsClient(boost::asio::io_service& io_service, servers_ptr const& servers) :
      timer(io_service, boost::posix_time::seconds(0)), io_service(io_service),
      multicast_endpoint(address::from_string(MDNS_ADDRESS_DEFAULT), MDNS_PORT_DEFAULT_NUM),
      udp_socket(io_service, multicast_endpoint.protocol()),
      servers(servers) {
    start_mdns_receiving();
    start_mdns_query(boost::system::error_code());


    // TODO to samo co u serwera!




/////////// TODO tego ma nie być
    udp::resolver resolver(io_service);
    udp::resolver::query query(udp::v4(), "localhost", "10001");
    std::shared_ptr<udp::endpoint> udp_endpoint_ptr(new udp::endpoint(*resolver.resolve(query)));
    std::shared_ptr<address> ip_ptr(new address(udp_endpoint_ptr->address()));
    servers->insert(std::make_pair(*ip_ptr, Server(ip_ptr, udp_endpoint_ptr,
                                                  std::shared_ptr<tcp::endpoint>(),
                                                  std::shared_ptr<icmp::endpoint>(), io_service)));

    udp::resolver::query query2(tcp::v4(), "mimuw.edu.pl", "80");
    std::shared_ptr<tcp::endpoint> tcp_endpoint_ptr(new tcp::endpoint(*resolver.resolve(query2)));
    std::shared_ptr<address> ip_ptr(new address(tcp_endpoint_ptr->address()));
    servers->insert(std::make_pair(*ip_ptr, Server(ip_ptr, std::shared_ptr<udp::endpoint>(),
                                                  tcp_endpoint_ptr,
                                                  std::shared_ptr<icmp::endpoint>(), io_service)));
  }


private:
  void start_mdns_query(const boost::system::error_code& error) {
    if (error)
      throw boost::system::system_error(error);

    std::cout << "mDNS query!\n";

    std::shared_ptr<std::string> message(new std::string("mDNS query"));
    udp_socket.async_send_to(boost::asio::buffer(*message), multicast_endpoint,
        boost::bind(&MdnsClient::handle_mdns_send, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));

    reset_timer(MDNS_INTERVAL_DEFAULT);
  }

  void handle_mdns_send(const boost::system::error_code& error,
      std::size_t /*bytes_transferred*/) {
    if (error)
      throw boost::system::system_error(error);
    std::cout << "mDNS query successfully sent!\n";
  }




  void start_mdns_receiving() {
    udp_socket.async_receive_from(
        boost::asio::buffer(recv_buffer), mdns_remote_endpoint,
        boost::bind(&MdnsClient::handle_mdns_receive, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  void handle_mdns_receive(const boost::system::error_code& error,
      std::size_t bytes_transferred) {
    if (error)
      throw boost::system::system_error(error);

    std::cout << "mDNS CLIENT: datagram received: [";
    std::cout.write(recv_buffer.data(), bytes_transferred);
    std::cout << "]\n";

    start_mdns_receiving();
  }


  /* Ustawia timer na czas późiejszy o 'seconds' sekund względem poprzedniego czasu. */
  void reset_timer(int seconds) {
    // TODO może jeden obiekt reprezentujący czas?
    timer.expires_at(timer.expires_at() + boost::posix_time::seconds(seconds));
    timer.async_wait(boost::bind(&MdnsClient::start_mdns_query, this,
        boost::asio::placeholders::error)); // TODO errors
    // TODO czy to działa?
  }


  boost::asio::deadline_timer timer;
  boost::array<char, BUFFER_SIZE> recv_buffer;

  boost::asio::io_service& io_service;
  udp::endpoint multicast_endpoint;
  udp::endpoint mdns_remote_endpoint;
  udp::socket udp_socket;

  servers_ptr servers;
};


#endif  // MDNS_CLIENT_H