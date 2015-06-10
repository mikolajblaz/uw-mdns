#ifndef MDNS_CLIENT_H
#define MDNS_CLIENT_H

#include <set>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common.h"
#include "server.h"
#include "mdns_message.h"

using boost::asio::ip::udp;
using boost::asio::ip::address;
using boost::asio::ip::address_v4;

class MdnsClient {
public:
  MdnsClient(boost::asio::io_service& io_service, servers_ptr servers,
      std::shared_ptr<udp::socket> udp_socket, std::shared_ptr<icmp::socket> icmp_socket,
      int mdns_interval, int mdns_port) :
          timer(io_service, boost::posix_time::seconds(0)),
          io_service(io_service),
          udp_socket(udp_socket),
          icmp_socket(icmp_socket),
          recv_buffer(),
          send_buffer(),
          recv_stream(&recv_buffer),
          send_stream(&send_buffer),
          multicast_endpoint(address::from_string(MDNS_ADDRESS), mdns_port),
          send_socket(io_service, multicast_endpoint.protocol()),
          recv_socket(io_service),
          servers(servers),
          known_udp_server_names(),
          known_tcp_server_names(),
          opoznienia_service(OPOZNIENIA_SERVICE),
          ssh_service(SSH_SERVICE),
          mdns_interval(mdns_interval) {
    try {
      /* dołączamy do grupy adresu 224.0.0.251, odbieramy na porcie 5353: */
      recv_socket.open(udp::v4());
      recv_socket.set_option(udp::socket::reuse_address(true));
      recv_socket.bind(udp::endpoint(udp::v4(), mdns_port));    // port 5353
      recv_socket.set_option(boost::asio::ip::multicast::join_group(
          address::from_string(MDNS_ADDRESS)));     // adres 224.0.0.251

      start_mdns_receiving();
      start_mdns_ptr_query();

    } catch (boost::system::error_code ec) {
      std::cerr << "Failed to start mDNS Client!\n";
    }
  }


private:
  /* Inicjuje zapytanie mdns typu PTR o usługę _opozenienia._udp.local,
   * które jest wysyłane w zadanych odstępach czasowych. */
  void start_mdns_ptr_query() {
    /* Zapytanie PTR _opoznienia._udp.local. oraz PTR _ssh._tcp.local */
    MdnsQuery query;
    query.add_question(opoznienia_service, QTYPE::PTR);
    query.add_question(ssh_service, QTYPE::PTR);
    send_mdns_query(query);

    reset_timer(mdns_interval);   // ustawienie licznika
  }

  /* Inicjuje jednorazowe zapytanie mdns typu A o ip servera o nazwie 'server_name'. */
  void start_mdns_a_query(MdnsDomainName server_name) {
    /* Zapytanie A _opoznienia._udp.local. */
    MdnsQuery query;
    query.add_question(server_name, QTYPE::A);
    send_mdns_query(query);
  }

  /* Wysyła zapytanie 'query': */
  void send_mdns_query(MdnsQuery const& query) {
    send_buffer.consume(send_buffer.size());  // wyczyść bufor
    send_stream << query;

    send_socket.async_send_to(send_buffer.data(), multicast_endpoint,
        boost::bind(&MdnsClient::handle_mdns_send, this));
  }

  void handle_mdns_send() {}


  /* Zlecenie odbioru pakietów multicastowych. */
  void start_mdns_receiving() {
    recv_buffer.consume(recv_buffer.size());  // wyczyść bufor

    recv_socket.async_receive_from(
        recv_buffer.prepare(BUFFER_SIZE), remote_endpoint,
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
  void handle_mdns_receive(boost::system::error_code const& error,
      std::size_t bytes_transferred) {
    if (!error) {
      MdnsResponse response;
      recv_buffer.commit(bytes_transferred);   // przygotowanie bufora

      try {
        if (response.try_read(recv_stream)) {       // ignorujemy pakiety mDNS typu 'Query'
          const std::vector<MdnsAnswer>& answers(response.get_answers());
          for (int i = 0; i < answers.size(); i++)
            handle_answer(answers[i]);
        }
      } catch (InvalidMdnsMessageException e) {}    // ignorujemy niepoprawne pakiety
    }

    start_mdns_receiving();
  }


  /* Obsługuje jedną odpowiedź otrzymaną w pakiecie mDNS aktualizując bazę serwerów. */
  void handle_answer(MdnsAnswer const& answer) {
    uint16_t type = answer.get_type();
    MdnsDomainName name(answer.get_name());
    if (type == static_cast<uint16_t>(QTYPE::PTR)) {  // w odpowiedzi jest nazwa serwera
    
      if (name == opoznienia_service) {     // serwer udostępnia _opoznienia._udp.local
        auto iter = known_udp_server_names.insert(answer.get_server_name());
        start_mdns_a_query(*iter.first);    // odpowiadamy zapytaniem typu A o adres serwera
      } else if (name == ssh_service) {     // serwer udostępnia _ssh.local
        auto iter = known_tcp_server_names.insert(answer.get_server_name());
        start_mdns_a_query(*iter.first);    // odpowiadamy zapytaniem typu A o adres serwera
      } // else nieznana usługa 

    } else if (type == static_cast<uint16_t>(QTYPE::A)) {
      /* sprawdzamy czy serwer udostępnia znane nam usługi: */
      bool is_udp_server = known_udp_server_names.find(name) != known_udp_server_names.end();
      bool is_tcp_server = known_tcp_server_names.find(name) != known_tcp_server_names.end();

      if (is_udp_server || is_tcp_server) {
        std::shared_ptr<address> server_address(new address(address_v4(answer.get_server_address())));
        /* jeśli jeszcze nie ma go w mapie, dodajemy go: */
        auto iter = servers->find(*server_address);
        if (iter == servers->end()) {
          auto tmp_it = servers->emplace(*server_address,
              Server(server_address, io_service, udp_socket, icmp_socket));
          iter = tmp_it.first;
        }

        if (is_udp_server)
          iter->second.enable_udp(answer.get_ttl());
        if (is_tcp_server)
          iter->second.enable_tcp(answer.get_ttl());
      }
    } // else nieznany typ odpowiedzi - ignorujemy
  }


  /* Ustawia timer na czas późniejszy o 'seconds' sekund względem poprzedniego czasu. */
  void reset_timer(int seconds) {
    timer.expires_at(timer.expires_at() + boost::posix_time::seconds(seconds));
    timer.async_wait(boost::bind(&MdnsClient::start_mdns_ptr_query, this));
  }


  boost::asio::deadline_timer timer;
  boost::asio::io_service& io_service;

  std::shared_ptr<udp::socket>  udp_socket;  // gniazdo używane do wszstkich pakietów UDP
  std::shared_ptr<icmp::socket> icmp_socket; // gniazdo używane do wszstkich pakietów ICMP

  boost::asio::streambuf recv_buffer; // bufor do odbierania
  boost::asio::streambuf send_buffer; // bufor do wysyłania
  std::istream recv_stream;           // strumień do odbierania
  std::ostream send_stream;           // strumień do wysyłania

  udp::endpoint multicast_endpoint;   // odbieranie na porcie 5353 z adresu 224.0.0.251
  udp::endpoint remote_endpoint;      // endpoint nadawcy odbieranego pakietu
  udp::socket send_socket;            // wysyłanie pakietów na multicast
  udp::socket recv_socket;            // odbieranie z multicastowych

  servers_ptr servers;
  std::set<MdnsDomainName> known_udp_server_names;  // zbiór znanych nazw serwerów udostępniających _opoznienia._udp
  std::set<MdnsDomainName> known_tcp_server_names;  // zbiór znanych nazw serwerów udostępniających _ssh.local
  const MdnsDomainName opoznienia_service;
  const MdnsDomainName ssh_service;

  int mdns_interval;
};


#endif  // MDNS_CLIENT_H