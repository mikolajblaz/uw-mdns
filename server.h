#ifndef SERVER_H
#define SERVER_H

#include <list>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common.h"
#include "get_time_usec.h"

using boost::asio::ip::address;
using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using boost::asio::ip::icmp;

class PrintServer;

class Server {
  friend PrintServer;
public:
  Server(std::shared_ptr<address> ip) : ip(ip) {
    std::cout << "New Server!!! ip: " << *ip << std::endl; // TODO remove
  }

  /* Aktywuje pomiary przez UDP i ICMP. */
  void enable_udp(uint32_t ttl) {
    udp_endpoint  = std::make_shared<udp::endpoint>(*ip, MDNS_PORT);
    icmp_endpoint = std::make_shared<icmp::endpoint>(*ip, MDNS_PORT);
    udp_ttl = get_time_usec() + ttl * SEC_TO_USEC;
  }
  /* Aktywuje pomiary przez TCP. */
  void enable_tcp(boost::asio::io_service& io_service, uint32_t ttl) {
    tcp_endpoint = std::make_shared<tcp::endpoint>(*ip, SSH_PORT);
    tcp_socket   = std::make_shared<tcp::socket>(io_service);
    tcp_ttl = get_time_usec() + ttl * SEC_TO_USEC;
  }
  /* Dezaktywuje pomiary przez UDP i ICMP. */
  void disable_udp() {
    udp_endpoint = std::shared_ptr<udp::endpoint>();
    icmp_endpoint = std::shared_ptr<icmp::endpoint>();
    udp_ttl = 0;
  }
  /* Dezaktywuje pomiary przez TCP. */
  void disable_tcp() {
    tcp_endpoint = std::shared_ptr<tcp::endpoint>();
    tcp_ttl = 0;
  }

  void send_queries(udp::socket& udp_socket, icmp::socket& icmp_socket) { // TODO może zapamiętać sobie gniazdo UDP i ICMP?
    time_type start_time = get_time_usec();
    /* jeśli wpisy się przedawniły, usuwamy je: */
    if (start_time > udp_ttl)
    if (udp_endpoint && start_time < udp_ttl) {
      send_udp_query(start_time, udp_socket);
      send_icmp_query(start_time, icmp_socket);
    }
    if (tcp_endpoint && start_time < tcp_ttl) {
      send_tcp_query(start_time);
    }
  }

  void receive_query(long id) {
    std::cout << *ip << ": RECEIVE query!\n";
  }

private:
  /* Wysyła pakiety UDP, TCP, ICMP do danego serwera. */
  void send_udp_query(time_type start_time, udp::socket& udp_socket) {
    std::cout << *ip << ": UDP query!\n";
    add_waiting_query(start_time, start_time, PROTOCOL::UDP);

    std::shared_ptr<std::string> message(new std::string("UDP query"));   // TODO pakiet
    udp_socket.async_send_to(boost::asio::buffer(*message), *udp_endpoint,
        boost::bind(&Server::handle_send, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
  }

  void send_tcp_query(time_type start_time) {
    std::cout << *ip << ": TCP query!\n";
    ++tcp_id;
    add_waiting_query(tcp_id, start_time, PROTOCOL::TCP);       // TODO chyba trzeba osobne sockety (>=10) :/

    tcp_socket->async_connect(*tcp_endpoint,
        boost::bind(&Server::receive_tcp_query, this,
            boost::asio::placeholders::error, tcp_id));   // TODO dobrze
  }
  void send_icmp_query(time_type start_time, icmp::socket& icmp_socket) {
    std::cout << *ip << ": ICMP query!\n";
    add_waiting_query(icmp_id, start_time, PROTOCOL::ICMP);
  }

  void add_waiting_query(long id, time_type start_time, int protocol) {  // TODO enum class ?
    if (waiting[protocol].size() >= MAX_DELAYED_QUERIES) {
      waiting[protocol].pop_back();    // porzuć najstarszy zaczęty pomiar
    }
    waiting[protocol].push_front(std::make_pair(id, start_time));
  }

  /* Uniwersalny handler po wysłaniu dla wszystkich protokołów. */
  void handle_send(boost::system::error_code const& error,
      std::size_t /*bytes_transferred*/) {
    if (error)
      throw boost::system::system_error(error);
    std::cout << *ip << ": Measurement query successfully sent!\n";
  }

public:     // TODO
  void receive_udp_query(boost::array<char, BUFFER_SIZE>& recv_buffer, time_type end_time) {
    std::cout << *ip << ": UDP RECEIVE query!\n";
    long extracted_start_time;
    //extracted_start_time = id;        // TODO
    finish_waiting_query(extracted_start_time, end_time, PROTOCOL::UDP);
  }
private:
  void receive_tcp_query(boost::system::error_code const& error, long id) {
    if (error)
      throw boost::system::system_error(error);
    std::cout << *ip << ": TCP RECEIVE query! ID[" << id << "]\n";
    finish_waiting_query(id, get_time_usec(), PROTOCOL::TCP);
    tcp_socket->close();
  }
  void receive_icmp_query(long id, time_type end_time) {
    std::cout << *ip << ": ICMP RECEIVE query!\n";
  }

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
    }
  }


  std::shared_ptr<address>        ip;
  std::shared_ptr<udp::endpoint>  udp_endpoint;
  std::shared_ptr<tcp::endpoint>  tcp_endpoint;
  std::shared_ptr<icmp::endpoint> icmp_endpoint;
  std::shared_ptr<tcp::socket>    tcp_socket;
  unsigned long tcp_id;
  unsigned long icmp_id;

  time_type udp_ttl;
  time_type tcp_ttl;

  std::list<time_type> finished[PROTOCOL_COUNT];
  std::list<std::pair<long, time_type> > waiting[PROTOCOL_COUNT];
  time_type delays_sum[PROTOCOL_COUNT];
};

#endif  // SERVER_H