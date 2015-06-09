#ifndef TELNET_SERVER_H
#define TELNET_SERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common.h"
#include "telnet_connection.h"
#include "print_server.h"

using boost::asio::ip::tcp;

class TelnetServer {
public:
  TelnetServer(boost::asio::io_service& io_service, servers_ptr const& servers) :
      io_service(io_service),
      timer(io_service, boost::posix_time::seconds(0)),
      tcp_acceptor(io_service, tcp::endpoint(tcp::v4(), UI_PORT_DEFAULT)),
      servers(servers),
      new_connection() {

    init_updates(boost::system::error_code());
    start_accept();
  }


private:
  /* Akceptuje nowe połączenia: */
  void start_accept() {
    new_connection = std::make_shared<TelnetConnection>(io_service, servers_table);

    tcp_acceptor.async_accept(new_connection->get_socket(),
        boost::bind(&TelnetServer::handle_accept, this,
          boost::asio::placeholders::error));
  }

  /* dołącza nowe połączenie do listy połączeń i uruchamia w nim komunikację. */
  void handle_accept(boost::system::error_code const& error) {
    if (error)
      throw boost::system::system_error(error);

    new_connection->activate();
    connections.push_back(new_connection);

    start_accept();
  }

  /* Inicjuje wysłanie pakietów aktualizujących ekran do wszystkich klientów telnet
   * oraz usuwa nieaktywne połączenia z listy połączeń. */
  void init_updates(boost::system::error_code const& error) {
    if (error)
      throw boost::system::system_error(error);

    float max_delay = build_servers_table();

    for (auto it = connections.begin(); it != connections.end();) {
      if ((*it)->is_active()) {
        (*it)->send_update(max_delay);       // odświeża ekran klienta
        ++it;
      } else {
        it = connections.erase(it);            // usuwa nieaktywne połączenie
      }
    }

    reset_timer(UI_REFRESH_INTERVAL_DEFAULT);
  }


  /* Ustawia timer na czas późniejszy o 'seconds' sekund względem poprzedniego czasu. */
  void reset_timer(float seconds) {
    // TODO może jeden obiekt reprezentujący czas?
    timer.expires_at(timer.expires_at() + boost::posix_time::microseconds(seconds * SEC_TO_USEC));
    timer.async_wait(boost::bind(&TelnetServer::init_updates, this,
        boost::asio::placeholders::error)); // TODO errors
    // TODO czy to działa?
  }

  /* Buduje tablicę drukowalnych i posortowanych serwerów i zwraca najdłuższe opóźnienie. */
  float build_servers_table() {
    servers_table.clear();
    servers_table.reserve(servers->size());
    for (auto it = servers->begin(); it != servers->end(); ++it) {
      servers_table.push_back(PrintServer((*it).second));
    }
    std::sort(servers_table.begin(), servers_table.end());

    static long long count = 0;
    std::cout << "TELNET: Init updates!\n";
    std::string message(fill_with_spaces(CLR_SCR + "Hello world, message, count: " + std::to_string(++count)));
    std::copy(message.begin(), message.end(), send_buffer.elems);
    // send_buffer[1] = "ziom\n";
    // send_buffer[2] = std::to_string(++count) + "\n";
    // send_buffer[3] = "3 rzad";
    // send_buffer[4] = "4 rząd\n";

    return servers_table.empty() ? 0 : servers_table[0].delay_sec();
  }

  std::string fill_with_spaces(std::string const& too_short) {
    return too_short + std::string(UI_SCREEN_WIDTH - too_short.size(), '.');
  }


  boost::asio::io_service& io_service;
  boost::asio::deadline_timer timer;
  boost::array<char, UI_SCREEN_WIDTH> send_buffer;
  tcp::acceptor tcp_acceptor;

  servers_ptr servers;
  std::list<std::shared_ptr<TelnetConnection> > connections;
  std::shared_ptr<TelnetConnection> new_connection;

  std::vector<PrintServer> servers_table;
};

#endif  // TELNET_SERVER_H