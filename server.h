#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <list>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <endian.h>
#include "common.h"
#include "get_time_usec.h"

/* ze strony http://www.boost.org/doc/libs/1_58_0/doc/html/boost_asio/examples/cpp03_examples.html */
#include "icmp_header.hpp"

using boost::asio::ip::address;
using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using boost::asio::ip::icmp;

class PrintServer;

/* Klasa reprezentująca komputer o danym IP, który jest serwuje usługę
 * _opoznienia._udp.local i/lub _ssh._tcp.local. Gromadzi informacje
 * o danym serwerze i o pomiarach do niego wysłanych i zakończonych. */
class Server {
  friend PrintServer;
public:
  Server(std::shared_ptr<address> ip, boost::asio::io_service& io_service,
      std::shared_ptr<udp::socket> udp_socket, std::shared_ptr<icmp::socket> icmp_socket) :
          ip(ip),
          io_service(io_service),
          send_buffer(),
          send_stream(&send_buffer),
          udp_endpoint(*ip, UDP_PORT_DEFAULT),
          icmp_endpoint(*ip, UDP_PORT_DEFAULT),
          tcp_endpoint(*ip, SSH_PORT),
          udp_socket(udp_socket),
          icmp_socket(icmp_socket),
          active_udp(false),
          active_tcp(false) {
    std::cout << "New Server!!! ip: " << *ip << std::endl; // TODO remove
  }

  Server(Server&& s) :
          ip(std::move(s.ip)),
          io_service(s.io_service),
          send_buffer(),                          // !!!
          send_stream(&send_buffer),              // !!!
          udp_endpoint(std::move(s.udp_endpoint)),
          icmp_endpoint(std::move(s.icmp_endpoint)),
          tcp_endpoint(std::move(s.tcp_endpoint)),
          udp_socket(std::move(s.udp_socket)),
          icmp_socket(std::move(s.icmp_socket)),
          active_udp(s.active_udp),
          active_tcp(s.active_tcp) {
    std::cout << "Copy Server!!! ip: " << *ip << std::endl; // TODO remove
  }

  /* Aktywuje pomiary przez UDP i ICMP. */
  void enable_udp(uint32_t ttl) {
    active_udp = true;
    udp_ttl = get_time_usec() + ttl * SEC_TO_USEC;
  }
  /* Aktywuje pomiary przez TCP. */
  void enable_tcp(uint32_t ttl) {
    active_tcp = true;
    tcp_ttl = get_time_usec() + ttl * SEC_TO_USEC;
  }
  /* Dezaktywuje pomiary przez UDP i ICMP. */
  void disable_udp() {
    active_udp = false;
    udp_ttl = 0;
    std::cout << "UDP Server DEACTIVATED :( !!! ip: " << *ip << std::endl; // TODO remove
  }
  /* Dezaktywuje pomiary przez TCP. */
  void disable_tcp() {
    active_udp = false;
    // TODO zamknij sockety
    tcp_ttl = 0;
    std::cout << "UDP Server DEACTIVATED :( !!! ip: " << *ip << std::endl; // TODO remove
  }

  /* Wysyła pakiety UDP, TCP, ICMP do aktywnych serwerów: */
  void send_queries() {
    time_type start_time = get_time_usec();
    /* jeśli wpisy się przedawniły, usuwamy je: */
    if (active_udp && start_time > udp_ttl)
      disable_udp();
    if (active_tcp && start_time > tcp_ttl)
      disable_tcp();
    
    if (active_udp) {
      send_udp_query(start_time);
      send_icmp_query(start_time);
    }
    if (active_tcp) {
      send_tcp_query(start_time);
    }
  }

  void receive_udp_query(time_type start_time, time_type end_time) {
    std::cout << *ip << ": UDP RECEIVE query with time: " << start_time << "!\n"; //TODO
    finish_waiting_query(start_time, end_time, PROTOCOL::UDP);
  }

  void receive_icmp_query(long id, time_type end_time) {
    std::cout << *ip << ": ICMP RECEIVE query!\n";
    finish_waiting_query(id, end_time, PROTOCOL::ICMP);
  }

private:
  void send_udp_query(time_type start_time) {
    std::cout << *ip << ": UDP query with time: " << start_time << "!\n";

    uint64_t be_start_time = htobe64(start_time);
    send_buffer.consume(send_buffer.size());
    send_stream.write(reinterpret_cast<const char *>(&be_start_time), sizeof(be_start_time));

    udp_socket->async_send_to(send_buffer.data(), udp_endpoint,
        boost::bind(&Server::handle_send, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));

    add_waiting_query(start_time, start_time, PROTOCOL::UDP);
  }

  void send_icmp_query(time_type start_time) {
    std::cout << *ip << ": ICMP query!\n";
    ++icmp_id;
    icmp_header icmp_header;
    std::string icmp_message(even_decimal_to_bcd(ICMP_MESSAGE));

    send_buffer.consume(send_buffer.size());
    icmp_header.sequence_number(icmp_id);
    compute_checksum(icmp_header, icmp_message.begin(), icmp_message.end());
    send_stream << icmp_header << icmp_message;

    icmp_socket->async_send_to(send_buffer.data(), icmp_endpoint,
        boost::bind(&Server::handle_send, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));

    add_waiting_query(icmp_id, start_time, PROTOCOL::ICMP);
  }

  void send_tcp_query(time_type start_time) {
    std::cout << *ip << ": TCP query!\n";
    ++tcp_id;

    tcp_sockets.push_front(tcp::socket(io_service, tcp::v4()));

    tcp_sockets.back().async_connect(tcp_endpoint,
        boost::bind(&Server::receive_tcp_query, this, tcp_id,
            boost::asio::placeholders::error));

    add_waiting_query(tcp_id, start_time, PROTOCOL::TCP);

    if (tcp_sockets.size() >= MAX_DELAYED_QUERIES)
      tcp_sockets.pop_back();            // usuwa pomiar jeśli jest za dużo
  }

  void receive_tcp_query(long id, boost::system::error_code const& error) {
    if (error) {
      unfinished_waiting_query(id, PROTOCOL::TCP);
    } else {
      std::cout << *ip << ": TCP RECEIVE query! ID[" << id << "]\n";
      finish_waiting_query(id, get_time_usec(), PROTOCOL::TCP);
    }
  }

  void add_waiting_query(long id, time_type start_time, int protocol) {  // TODO enum class ?
    if (waiting[protocol].size() >= MAX_DELAYED_QUERIES) {
      waiting[protocol].pop_back();    // porzuć najstarszy zaczęty pomiar
    }
    waiting[protocol].push_front(std::make_pair(id, start_time));
  }

  /* Uniwersalny handler po wysłaniu protokołów UDP i ICMP. */
  void handle_send(boost::system::error_code const& error,          // TODO usunąć wszystkie placeholders! NA CO MI ONE!!!!!!
      std::size_t /*bytes_transferred*/) {
    if (error)
      throw boost::system::system_error(error);
    std::cout << *ip << ": Measurement query successfully sent!\n";
  }

  /* Kończy pomiar o identyfikatorze 'id'. */
  void finish_waiting_query(long id, time_type end_time, int protocol) {   //TODO classas enum?
    time_type diff_time;
    auto init_query = waiting[protocol].begin();
    while (init_query != waiting[protocol].end() && init_query->first != id)
      ++init_query;

    if (init_query != waiting[protocol].end()) {   // znaleziono; else ignoruj pomiar
      diff_time = end_time - init_query->second;
      waiting[protocol].erase(init_query);
      if (finished[protocol].size() >= AVERAGED_MEASUREMENTS) {
        /* usuń najstarszy skończony pomiar: */
        delays_sum[protocol] -= finished[protocol].back();
        finished[protocol].pop_back();
      }
      finished[protocol].push_front(diff_time);
      delays_sum[protocol] += diff_time;
    }
  }

  /* Obsługuje nieukończony pomiar o identyfikatorze 'id'. */
  void unfinished_waiting_query(long id, int protocol) {   //TODO classas enum?
    auto init_query = waiting[protocol].begin();
    while (init_query != waiting[protocol].end() && init_query->first != id)
      ++init_query;

    if (init_query != waiting[protocol].end()) {   // znaleziono; else ignoruj pomiar
      waiting[protocol].erase(init_query);
      if (finished[protocol].size() >= AVERAGED_MEASUREMENTS) {
        /* usuń najstarszy skończony pomiar: */
        delays_sum[protocol] -= finished[protocol].back();
        finished[protocol].pop_back();
      }
      finished[protocol].push_front(MAX_DELAY_TIME * SEC_TO_USEC);
      delays_sum[protocol] += MAX_DELAY_TIME * SEC_TO_USEC;
    }
  }

  /* Konwertuje liczbę w zapisie 10 o parzystej liczbie cyfr do systemu BCD. */
  std::string even_decimal_to_bcd(std::string const& decimal) {
    std::string result(decimal.size() / 2, '\0');
    for (int i = 0; i < decimal.size(); i++)
      result[i / 2] += ((decimal[i] - '0') & 0x0F) << (i % 2 ? 0 : 4);  // shift parzystych
    return result;
  }



  std::shared_ptr<address> ip;
  boost::asio::io_service& io_service;
  boost::asio::streambuf send_buffer; // bufor do wysyłania
  std::ostream send_stream;           // strumień do wysyłania

  udp::endpoint  udp_endpoint;
  icmp::endpoint icmp_endpoint;
  tcp::endpoint  tcp_endpoint;
  std::shared_ptr<udp::socket>    udp_socket;  // gniazdo używane do wszstkich pakietów UDP
  std::shared_ptr<icmp::socket>   icmp_socket; // gniazdo używane do wszstkich pakietów ICMP
  std::list<tcp::socket>          tcp_sockets;

  bool active_udp;                    // czy pomiary UDP i ICMP są aktywne
  bool active_tcp;                    // czy pomiary TCP są aktywne
  time_type udp_ttl;                  // TTL serwera UDP
  time_type tcp_ttl;                  // TTL serwera TCP

  unsigned long tcp_id;
  unsigned long icmp_id;

  std::list<time_type> finished[PROTOCOL_COUNT];    // lista ukończonych pomiarów
  std::list<std::pair<long, time_type> > waiting[PROTOCOL_COUNT]; // lista oczekujących pomiarów
  time_type delays_sum[PROTOCOL_COUNT];             // suma opóźnień
};

#endif  // SERVER_H