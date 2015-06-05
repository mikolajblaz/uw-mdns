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

class Server {
public:
  Server(std::shared_ptr<address> ip, std::shared_ptr<udp::endpoint> udp_endpoint,
      std::shared_ptr<tcp::endpoint> tcp_endpoint, std::shared_ptr<icmp::endpoint> icmp_endpoint,
      boost::asio::io_service& io_service) :
          ip(ip), udp_endpoint(udp_endpoint), tcp_endpoint(tcp_endpoint), icmp_endpoint(icmp_endpoint),
          tcp_socket(io_service), tcp_id(0), icmp_id(0) {}

  void send_queries(udp::socket& udp_socket, icmp::socket& icmp_socket) { // TODO może zapamiętać sobie gniazdo UDP i ICMP?
    // TODO nie za często te wywołania get_time_usec?
    time_type start_time = get_time_usec();
    if (udp_endpoint)  send_udp_query(start_time, udp_socket);
    if (tcp_endpoint)  send_tcp_query(start_time);
    if (icmp_endpoint) send_icmp_query(start_time, icmp_socket);
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
    add_waiting_query(tcp_id, start_time, PROTOCOL::TCP);       // TODO chyba trzeba osobne sockety :/

    tcp_socket.async_connect(*tcp_endpoint,
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
  void handle_send(const boost::system::error_code& error,
      std::size_t /*bytes_transferred*/) {
    if (error)
      throw boost::system::system_error(error);
    std::cout << *ip << ": Measurement query successfully sent!\n";
  }



  void receive_udp_query(long id, time_type end_time) {
    std::cout << *ip << ": UDP RECEIVE query!\n";
    long extracted_start_time;
    extracted_start_time = id;
    finish_waiting_query(extracted_start_time, end_time, PROTOCOL::UDP);
  }
  void receive_tcp_query(const boost::system::error_code& error, long id) {
    if (error)
      throw boost::system::system_error(error);
    std::cout << *ip << ": TCP RECEIVE query!\n";
    finish_waiting_query(id, get_time_usec(), PROTOCOL::TCP);
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
  tcp::socket tcp_socket;
  unsigned long tcp_id;
  unsigned long icmp_id;

  std::list<time_type> finished[PROTOCOL_COUNT];
  std::list<std::pair<long, time_type> > waiting[PROTOCOL_COUNT];
  time_type delays_sum[PROTOCOL_COUNT];
};

#endif  // SERVER_H