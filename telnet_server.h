#ifndef TELNET_SERVER_H
#define TELNET_SERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common.h"
#include "telnet_connection.h"
#include "server.h"
#include "print_server.h"

using boost::asio::ip::tcp;

class TelnetServer {
public:
  TelnetServer(boost::asio::io_service& io_service, servers_ptr servers) :
      io_service(io_service),
      timer(io_service, boost::posix_time::seconds(0)),
      tcp_acceptor(io_service, tcp::endpoint(tcp::v4(), UI_PORT_DEFAULT)),
      servers(servers),
      new_connection() {

    init_updates();
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
    if (!error) {
      new_connection->activate();
      connections.push_back(new_connection);
    }

    start_accept();
  }

  /* Inicjuje wysłanie pakietów aktualizujących ekran do wszystkich klientów telnet
   * oraz usuwa nieaktywne połączenia z listy połączeń. */
  void init_updates() {
    build_servers_table();

    for (auto it = connections.begin(); it != connections.end();) {
      if ((*it)->is_active()) {
        (*it)->send_update();                  // odświeża ekran klienta
        ++it;
      } else {
        it = connections.erase(it);            // usuwa nieaktywne połączenie
      }
    }

    reset_timer(UI_REFRESH_INTERVAL_DEFAULT);
  }


  /* Ustawia timer na czas późniejszy o 'seconds' sekund względem poprzedniego czasu. */
  void reset_timer(float seconds) {
    timer.expires_at(timer.expires_at() + boost::posix_time::microseconds(seconds * SEC_TO_USEC));
    timer.async_wait(boost::bind(&TelnetServer::init_updates, this));
  }

  /* Buduje tablicę drukowalnych i posortowanych serwerów. */
  void build_servers_table() {
    const float max_delay = MAX_DELAY_TIME;     // opóźnienie w sekundach

    servers_table.clear();
    servers_table.reserve(servers->size());
    for (auto it = servers->begin(); it != servers->end(); ++it) {
      servers_table.push_back(PrintServer((*it).second, max_delay));
    }
    /* Sortujemy malejąco po czasach: */
    std::sort(servers_table.begin(), servers_table.end());
  }


  boost::asio::io_service& io_service;
  boost::asio::deadline_timer timer;
  tcp::acceptor tcp_acceptor;

  servers_ptr servers;
  std::list<std::shared_ptr<TelnetConnection> > connections;
  std::shared_ptr<TelnetConnection> new_connection;

  std::vector<PrintServer> servers_table;
};

#endif  // TELNET_SERVER_H