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
      std::shared_ptr<tcp::endpoint> tcp_endpoint, std::shared_ptr<icmp::endpoint> icmp_endpoint) :
          ip(ip), udp_endpoint(udp_endpoint), tcp_endpoint(tcp_endpoint), icmp_endpoint(icmp_endpoint) {}

  void send_queries(udp::socket& udp_socket, tcp::socket& tcp_socket, icmp::socket& icmp_socket) {
    // TODO nie za często te wywołania get_time_usec?
    time_type start_time = get_time_usec();
    if (udp_endpoint)  send_udp_query(start_time, udp_socket);
    if (tcp_endpoint)  send_tcp_query(start_time, tcp_socket);
    if (icmp_endpoint) send_icmp_query(start_time, icmp_socket);
  }

  void receive_query(long id) {
    std::cout << *ip << ": RECEIVE query!\n";
  }

private:
  std::shared_ptr<address>        ip;
  std::shared_ptr<udp::endpoint>  udp_endpoint;
  std::shared_ptr<tcp::endpoint>  tcp_endpoint;
  std::shared_ptr<icmp::endpoint> icmp_endpoint;

  std::list<time_type> finished[PROTOCOL_COUNT];
  std::list<std::pair<long, time_type> > waiting[PROTOCOL_COUNT];
  time_type delays_sum[PROTOCOL_COUNT];

  /* Wysyła pakiety UDP, TCP, ICMP do danego serwera. */
  void send_udp_query(time_type start_time, udp::socket& udp_socket) {
    std::cout << *ip << ": UDP query!\n";
    if (waiting[PROTOCOL::UDP].size() >= MAX_DELAYED_QUERIES) {
      waiting[PROTOCOL::UDP].pop_back();    // porzuć najstarszy zaczęty pomiar
    }
    waiting[PROTOCOL::UDP].push_front(std::make_pair(start_time, start_time));

    std::shared_ptr<std::string> message(new std::string("UDP query"));
    udp_socket.async_send_to(boost::asio::buffer(*message), *udp_endpoint,
        boost::bind(&Server::handle_send, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
  }

  void send_tcp_query(time_type start_time, tcp::socket& tcp_socket) {
    std::cout << *ip << ": TCP query!\n";
  }
  void send_icmp_query(time_type start_time, icmp::socket& icmp_socket) {
    std::cout << *ip << ": ICMP query!\n";
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
    time_type diff_time;
    auto init_query = waiting[PROTOCOL::UDP].begin();
    while (init_query != waiting[PROTOCOL::UDP].end() && init_query->first != id)
      ++init_query;

    if (init_query != waiting[PROTOCOL::UDP].end()) {   // znaleziono; else ignoruj pomiar
      diff_time = end_time - init_query->second;
      waiting[PROTOCOL::UDP].erase(init_query);
      if (finished[PROTOCOL::UDP].size() >= AVERAGED_MEASUREMENTS) {
        /* usuń najstarszy skończony pomiar: */
        delays_sum[PROTOCOL::UDP] -= finished[PROTOCOL::UDP].back();
        finished[PROTOCOL::UDP].pop_back();
      }
      finished[PROTOCOL::UDP].push_front(diff_time);
    }
  }
  void receive_tcp_query(long id, time_type end_time) {
    std::cout << *ip << ": TCP RECEIVE query!\n";
  }
  void receive_icmp_query(long id, time_type end_time) {
    std::cout << *ip << ": ICMP RECEIVE query!\n";
  }

};

#endif  // SERVER_H