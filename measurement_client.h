#ifndef MEASUREMENT_CLIENT_H
#define MEASUREMENT_CLIENT_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common.h"
#include "get_time_usec.h"    // TODO włączyć do common
#include "server.h"

using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using boost::asio::ip::icmp;

class MeasurementClient {
public:
  MeasurementClient(boost::asio::io_service& io_service, servers_ptr const& servers) :    // TODO const& czy value?
      timer(io_service, boost::posix_time::seconds(0)),
      udp_socket(io_service, udp::v4()), icmp_socket(io_service),
      servers(servers) {

    start_udp_receiving();
    start_icmp_receiving();

    init_measurements(boost::system::error_code());
  }


private:
  /* Inicjuje wysłanie pakietów rozpoczynających pomiar do wszystkich serwerów. */
  void init_measurements(const boost::system::error_code& error) {
    if (error)
      throw boost::system::system_error(error);

    std::cout << "Init measurements!\n";
    for (auto it = servers->begin(); it != servers->end(); ++it) {
      it->second.send_queries(udp_socket, icmp_socket);
    }

    reset_timer(MEASUREMENT_INTERVAL_DEFAULT);
  }


  void start_udp_receiving() {
    udp_socket.async_receive_from(
        boost::asio::buffer(recv_buffer), remote_udp_endpoint,
        boost::bind(&MeasurementClient::handle_udp_receive, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  void handle_udp_receive(const boost::system::error_code& error,
      std::size_t bytes_transferred) {
    if (error || bytes_transferred < sizeof(uint64_t))
      throw boost::system::system_error(error);
    time_type end_time = get_time_usec();

    std::cout << "CLIENT: odebrano pakiet UDP: ";
    std::cout.write(recv_buffer.data(), bytes_transferred);
    std::cout << " od adresu " << remote_udp_endpoint << std::endl;

    auto it = servers->find(remote_udp_endpoint.address());
    if (it != servers->end()) { // else ignoruj pakiet
      it->second.receive_udp_query(recv_buffer, end_time); //TODO
    }

    start_udp_receiving();
  }


  void start_icmp_receiving() {
    // TODO
  }

  void handle_icmp_receive(const boost::system::error_code& error,
      std::size_t /*bytes_transferred*/) {
    if (error)
      throw boost::system::system_error(error);

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
  boost::array<char, BUFFER_SIZE> recv_buffer;

  udp::socket  udp_socket;
  icmp::socket icmp_socket;
  udp::endpoint remote_udp_endpoint;

  servers_ptr servers;
};

#endif  // MEASUREMENT_CLIENT_H