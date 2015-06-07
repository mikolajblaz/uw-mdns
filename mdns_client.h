#ifndef MDNS_CLIENT_H
#define MDNS_CLIENT_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common.h"
#include "mdns_message.h"

using boost::asio::ip::udp;
using boost::asio::ip::address;

class MdnsClient {
public:
  MdnsClient(boost::asio::io_service& io_service, servers_ptr const& servers) :
      timer(io_service, boost::posix_time::seconds(0)), io_service(io_service),
      multicast_endpoint(address::from_string(MDNS_ADDRESS), MDNS_PORT),
      send_socket(io_service, multicast_endpoint.protocol()),
      recv_socket(io_service), servers(servers) {
    /* dołączamy do grupy adresu 224.0.0.251, odbieramy na porcie 5353: */
    recv_socket.open(udp::v4());
    recv_socket.set_option(udp::socket::reuse_address(true));
    recv_socket.bind(udp::endpoint(udp::v4(), MDNS_PORT));    // port 5353
    recv_socket.set_option(boost::asio::ip::multicast::join_group(
        address::from_string(MDNS_ADDRESS)));     // adres 224.0.0.251

    /* nie pozwalamy na wysyłanie do siebie: */
    boost::asio::ip::multicast::enable_loopback option(false);
    //send_socket.set_option(option);     // TODO turn on?

    start_mdns_receiving();
    start_mdns_query(boost::system::error_code());




/////////// TODO tego ma nie być
    udp::resolver resolver(io_service);
    udp::resolver::query query(udp::v4(), "localhost", "10001");
    std::shared_ptr<udp::endpoint> udp_endpoint_ptr(new udp::endpoint(*resolver.resolve(query)));
    std::shared_ptr<address> ip_ptr(new address(udp_endpoint_ptr->address()));
    servers->insert(std::make_pair(*ip_ptr, Server(ip_ptr, udp_endpoint_ptr,
                                                  std::shared_ptr<tcp::endpoint>(),
                                                  std::shared_ptr<icmp::endpoint>(), io_service)));

    /*tcp::resolver resolver2(io_service);
    tcp::resolver::query query2(tcp::v4(), "mimuw.edu.pl", "80");
    std::shared_ptr<tcp::endpoint> tcp_endpoint_ptr(new tcp::endpoint(*resolver2.resolve(query2)));
    std::shared_ptr<address> ip_ptr2(new address(tcp_endpoint_ptr->address()));
    servers->insert(std::make_pair(*ip_ptr2, Server(ip_ptr2, std::shared_ptr<udp::endpoint>(),
                                                  tcp_endpoint_ptr,
                                                  std::shared_ptr<icmp::endpoint>(), io_service)));
    */
  }


private:
  /* Zapytanie mdns typu PTR o usługę _opozenienia._udp.local wysyłane
     w zadanych odstępach czasowych. */
  void start_mdns_query(const boost::system::error_code& error) {
    if (error)
      throw boost::system::system_error(error);

    std::cout << "mDNS CLIENT: mDNS query!\n";

    /* Zapytanie PTR _opoznienia._udp.local. */
    MdnsQuery query;
    query.add_question(OPOZNIENIA_SERVICE, QTYPE::PTR); 

    /* stworzenie pakietu: */
    boost::asio::streambuf request_buffer;    // TODO można chyba skorzystać z gotowego
    std::ostream os(&request_buffer);
    os << query;

    send_socket.async_send_to(request_buffer.data(), multicast_endpoint,
        boost::bind(&MdnsClient::handle_mdns_send, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));

    reset_timer(MDNS_INTERVAL_DEFAULT);   // ustawienie licznika
  }

  void handle_mdns_send(const boost::system::error_code& error,
      std::size_t /*bytes_transferred*/) {
    if (error)
      throw boost::system::system_error(error);
    std::cout << "mDNS CLIENT: mDNS query successfully sent!\n";
  }



  /* Zlecenie odbioru pakietów multicastowych. */
  void start_mdns_receiving() {
    recv_stream_buffer.consume(recv_stream_buffer.size());  // wyczyść bufor

    recv_socket.async_receive_from(
        recv_stream_buffer.prepare(BUFFER_SIZE), remote_endpoint,
        boost::bind(&MdnsClient::handle_mdns_receive, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  /* Próbujemy przeczytać pakiet mDNS typu 'Response'. Zapytanie typu 'Query'
   * jest ignorowane, zaś niepoprawne zapytanie typu 'Response' powoduje
   * rzucenie (i złapanie) wyjątku.
   *
   * Jeśli pakiet jest odpowiedzią typu:
   * PTR - dopisuje nazwę nowego serwera do zbioru znanych nazw
   * A - odświeża TTL serwera lub tworzy instancję klasy Server reprezentującą
   *     go, jeśli jeszcze nie istnieje (lub dodaje nowy rodzaj protokołu).
   */
  void handle_mdns_receive(const boost::system::error_code& error,
      std::size_t bytes_transferred) {
    if (error)
      throw boost::system::system_error(error);

    recv_stream_buffer.commit(bytes_transferred);   // przygotowanie bufora
    std::istream is(&recv_stream_buffer);
    MdnsResponse response;

    try {
      if (response.try_read(is)) {          // ignorujemy pakiety mDNS typu 'Query'
        std::cout << "mDNS CLIENT: datagram received: [" << response << "]\n";
      }

    } catch (InvalidMdnsMessageException e) {
      std::cout << "mDNS CLIENT: mDNS CLIENT: Ignoring packet... reason: " << e.what() << std::endl;
    }

    start_mdns_receiving();
  }


  /* Ustawia timer na czas późniejszy o 'seconds' sekund względem poprzedniego czasu. */
  void reset_timer(int seconds) {
    // TODO może jeden obiekt reprezentujący czas?
    timer.expires_at(timer.expires_at() + boost::posix_time::seconds(seconds));
    timer.async_wait(boost::bind(&MdnsClient::start_mdns_query, this,
        boost::asio::placeholders::error)); // TODO errors
    // TODO czy to działa?
  }


  boost::asio::deadline_timer timer;
  boost::array<char, BUFFER_SIZE> recv_buffer;
  boost::asio::streambuf recv_stream_buffer;

  boost::asio::io_service& io_service;
  udp::endpoint multicast_endpoint;   // odbieranie na porcie 5353 z adresu 224.0.0.251
  udp::endpoint remote_endpoint;      // endpoint nadawcy odbieranego pakietu
  udp::socket send_socket;            // wysyłanie pakietów na multicast
  udp::socket recv_socket;            // odbieranie z multicastowych

  servers_ptr servers;
};


#endif  // MDNS_CLIENT_H