#ifndef MDNS_SERVER_H
#define MDNS_SERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include "common.h"
#include "mdns_message.h"

using boost::asio::ip::udp;
using boost::asio::ip::address;

class MdnsServer {
public:
  MdnsServer(boost::asio::io_service& io_service) :
      multicast_endpoint(address::from_string(MDNS_ADDRESS), MDNS_PORT),
      send_socket(io_service, multicast_endpoint.protocol()),
      recv_socket(io_service) {
    /* dołączamy do grupy adresu 224.0.0.251, odbieramy na porcie 5353: */
    recv_socket.open(udp::v4());
    recv_socket.set_option(udp::socket::reuse_address(true));
    recv_socket.bind(udp::endpoint(udp::v4(), MDNS_PORT));    // port 5353
    recv_socket.set_option(boost::asio::ip::multicast::join_group(
        address::from_string(MDNS_ADDRESS)));     // adres 224.0.0.251

    /* nie pozwalamy na wysyłanie do siebie: */
    boost::asio::ip::multicast::enable_loopback option(false);
    //send_socket.set_option(option);     // TODO turn on?

    start_receive();
  }

private:
  /* zlecenie asynchronicznego odbioru pakietów multicastowych */
  void start_receive() {
    recv_socket.async_receive_from(
        recv_stream_buffer.prepare(BUFFER_SIZE), remote_endpoint,
        boost::bind(&MdnsServer::handle_receive, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  /* obsługa odebranego pakietu */
  void handle_receive(const boost::system::error_code& error,
      std::size_t bytes_transferred) {
    if (error)
      throw boost::system::system_error(error);

    recv_stream_buffer.commit(bytes_transferred);   // przygotowanie bufora

    try {
      std::istream is(&recv_stream_buffer);
      MdnsQuery query;
      is >> query;                  // TODO żeby dało się oba wczytać jakoś

      std::cout << "mDNS SERVER: datagram received: [" << query << "] from: " << remote_endpoint << "\n";

      /* odpowiedź: */
      MdnsResponse response = respond_to(query);
      boost::asio::streambuf request_buffer;
      std::ostream os(&request_buffer);
      os << query;

      send_socket.async_send_to(request_buffer.data(), multicast_endpoint,
          boost::bind(&MdnsServer::handle_send, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));

    } catch (InvalidMdnsMessageException e) {
      std::cout << "mDNS SERVER: Ignoring packet... reason: " << e.what() << std::endl;
    }

    start_receive();
  }

  void handle_send(const boost::system::error_code& error,
      std::size_t /*bytes_transferred*/) {
    if (error) {
      std::cout << "mDNS SERVER: Error in sending DNS response!\n";
      throw boost::system::system_error(error);
    } else {
      std::cout << "mDNS SERVER: Sent DNS response!" << std::endl;
    }
  }


  MdnsResponse respond_to(MdnsQuery const& query) {
    return MdnsResponse();
  }

  boost::array<char, BUFFER_SIZE> recv_buffer;
  boost::asio::streambuf recv_stream_buffer;

  udp::endpoint multicast_endpoint;   // odbieranie na porcie 5353 z adresu 224.0.0.251
  udp::endpoint remote_endpoint;      // endpoint nadawcy odbieranego pakietu
  udp::socket send_socket;            // wysyłanie pakietów na multicast
  udp::socket recv_socket;            // odbieranie z multicastowych
};

#endif  // MDNS_SERVER_H