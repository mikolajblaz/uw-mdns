#ifndef MEASUREMENT_CLIENT_H
#define MEASUREMENT_CLIENT_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <endian.h>
#include "common.h"
#include "get_time_usec.h"    // TODO włączyć do common
#include "mdns_client.h"
#include "telnet_server.h"
#include "server.h"
#include "mdns_message.h"

/* ze strony http://www.boost.org/doc/libs/1_58_0/doc/html/boost_asio/examples/cpp03_examples.html */
#include "ipv4_header.hpp"
#include "icmp_header.hpp"


using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using boost::asio::ip::icmp;

/* Centralna klasa pomiarów opóźnień. Zawiera klienta mDNS i serwer telnetu.
 * Jest odowiedzialna za odbieranie pakietów UDP i ICMP oraz delegowanie
 * ich do odpowiednich instancji klasy Server w mapie 'servers'.  */
class MeasurementClient {
public:
  MeasurementClient(boost::asio::io_service& io_service) :
      timer(io_service, boost::posix_time::seconds(0)),
      recv_buffer(),
      recv_stream(&recv_buffer),
      udp_socket(new udp::socket(io_service, udp::v4())),
      icmp_socket(new icmp::socket(io_service, icmp::v4())),
      servers(new servers_map),
      mdns_client(io_service, servers, udp_socket, icmp_socket),
      telnet_server(io_service, servers) {

    start_udp_receiving();
    start_icmp_receiving();

    init_measurements(boost::system::error_code());
  }


private:
  /* Inicjuje wysłanie pakietów rozpoczynających pomiar do wszystkich serwerów. */
  void init_measurements(boost::system::error_code const& error) {
    if (error)
      throw boost::system::system_error(error);

    std::cout << "Init measurements!\n";
    for (auto it = servers->begin(); it != servers->end(); ++it) {
      it->second.send_queries();
    }

    reset_timer(MEASUREMENT_INTERVAL_DEFAULT);
  }

  /* słuchanie na wspólnym porcie UDP. */
  void start_udp_receiving() {
    recv_buffer.consume(recv_buffer.size());    // wyczyść bufor

    udp_socket->async_receive_from(recv_buffer.prepare(BUFFER_SIZE), remote_udp_endpoint,
        boost::bind(&MeasurementClient::handle_udp_receive, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  void handle_udp_receive(boost::system::error_code const& error,
      std::size_t bytes_transferred) {
    if (error || bytes_transferred < sizeof(uint64_t))
      throw boost::system::system_error(error);

    time_type end_time = get_time_usec();
    recv_buffer.commit(bytes_transferred);

    uint64_t be_start_time;
    recv_stream.read(reinterpret_cast<char *>(&be_start_time), sizeof(be_start_time));

    std::cout << "CLIENT: odebrano pakiet UDP: ";
    std::cout << be64toh(be_start_time);
    
    std::cout << " od adresu " << remote_udp_endpoint << std::endl;

    auto it = servers->find(remote_udp_endpoint.address());
    if (it != servers->end()) { // else ignoruj pakiet
      it->second.receive_udp_query(be64toh(be_start_time), end_time); //TODO
    }

    start_udp_receiving();
  }

  /* słuchanie na wspólnym porcie ICMP. */
  void start_icmp_receiving() {
    recv_buffer.consume(recv_buffer.size());

    icmp_socket->async_receive(recv_buffer.prepare(BUFFER_SIZE),
        boost::bind(&MeasurementClient::handle_icmp_receive, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  void handle_icmp_receive(boost::system::error_code const& error,
      std::size_t bytes_transferred) {
    if (error)
      throw boost::system::system_error(error);

    time_type end_time = get_time_usec();
    recv_buffer.commit(bytes_transferred);

    ipv4_header ipv4_hdr;
    icmp_header icmp_hdr;
    recv_stream >> ipv4_hdr >> icmp_hdr;

    if (recv_stream && icmp_hdr.type() == icmp_header::echo_reply
          && icmp_hdr.identifier() == 0) {
      std::cout << "CLIENT: odebrano pakiet ICMP: ";
      //std::cout.write(recv_buffer.rdbuf(), bytes_transferred);
      std::cout << " od adresu " << ipv4_hdr.source_address() << std::endl;

      int seq_num = icmp_hdr.sequence_number();    // numer sekwencyjny jako id pakietu

      auto it = servers->find(ipv4_hdr.source_address());
      if (it != servers->end()) { // else ignoruj pakiet
        it->second.receive_icmp_query(seq_num, end_time);
      }
    }

    start_icmp_receiving();
  }

  /* Ustawia timer na czas późiejszy o 'seconds' sekund względem poprzedniego czasu. */
  void reset_timer(int seconds) {
    // TODO może jeden obiekt reprezentujący czas?
    timer.expires_at(timer.expires_at() + boost::posix_time::seconds(seconds));
    timer.async_wait(boost::bind(&MeasurementClient::init_measurements, this,
        boost::asio::placeholders::error)); // TODO errors
    // TODO czy to działa?
  }



  boost::asio::deadline_timer timer;

  boost::asio::streambuf recv_buffer; // bufor do odbierania
  std::istream recv_stream;           // strumień do odbierania

  std::shared_ptr<udp::socket>  udp_socket;  // gniazdo używane do wszstkich pakietów UDP
  std::shared_ptr<icmp::socket> icmp_socket; // gniazdo używane do wszstkich pakietów ICMP
  udp::endpoint remote_udp_endpoint;

  servers_ptr servers;


  MdnsClient mdns_client;
  TelnetServer telnet_server;
};

#endif  // MEASUREMENT_CLIENT_H