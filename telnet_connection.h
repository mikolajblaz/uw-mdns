#ifndef TELNET_CONNECTION_H
#define TELNET_CONNECTION_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common.h"
#include "get_time_usec.h"
#include "print_server.h"

const std::string IAC_WILL_SGA  = "\377\373\003";
const std::string IAC_WILL_ECHO = "\377\373\001";
const std::string CLR_SCR       = "\033[2J\033[H";

const unsigned char KEY_UP   = 'q';
const unsigned char KEY_DOWN = 'a';

using boost::asio::ip::tcp;

class TelnetConnection {
public:
  TelnetConnection(boost::asio::io_service& io_service, std::vector<PrintServer> const& servers_table) :
    send_buffer(),
    send_stream(&send_buffer),
    socket(io_service),
    active(false),
    servers_table(servers_table) {}

  tcp::socket& get_socket() { return socket; }
  bool is_active() const { return active; }

  /* Aktywuje nowo ustanowione połączenie. */
  void activate() {
    active = true;
    negotiate_options(IAC_WILL_SGA + IAC_WILL_ECHO);
    send_update();
    start_receive();
  }
  /* Dezaktywuje połączenie. */
  void deactivate() {
    active = false;
    socket.close();
  }

  /* Nasłuchuje wiadomości od klienta. */
  void start_receive() {
    socket.async_receive(boost::asio::buffer(recv_buffer),
        boost::bind(&TelnetConnection::handle_receive, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  /* Odświeża ekran klienta */
  void send_update() {
    send_buffer.consume(send_buffer.size());  // wyczyść bufor
    
    /* Wysyłamy znaki CLR_SCR i kolejno 24 wiersze tabelki. */
    send_stream << CLR_SCR;
    for (int i = table_position; i < servers_table.size() && i < table_position + UI_SCREEN_HEIGHT; ++i) {
      send_stream << servers_table[i];
    }

    boost::asio::async_write(socket, send_buffer.data(),
        boost::bind(&TelnetConnection::handle_send, this));
  }

private:
  /* Obsługa odebranych danych od klienta. */
  void handle_receive(boost::system::error_code const& error,
      std::size_t bytes_transferred) {
    if (error) {
      deactivate();   // koniec połączenia
    } else {

      for (auto it = recv_buffer.begin(); it != recv_buffer.end(); ++it) {
        handle_keypress(*it);
      }

      send_update();
      start_receive();
    }
  }

  /* Ibsługa naciśnięcie jednego przycisku przez klienta. */
  void handle_keypress(unsigned char key) {
    if (key == KEY_UP) {
      if (table_position > 0) {
        table_position--;
      }
    } else if (key == KEY_DOWN) {
      if (table_position < servers_table.size() - 1) {
        table_position++;
      }
    }
  }

  /* Funkcja negocjująca odpowiednie opcje z klientem telnet. */
  void negotiate_options(std::string const& options) {
    boost::system::error_code error;
    socket.send(boost::asio::buffer(options), 0, error);
  }

  void handle_send() {}


  boost::array<char, 2> recv_buffer;  // bufor do odbierania znaków
  boost::asio::streambuf send_buffer; // bufor do wysyłania
  std::ostream send_stream;           // strumień do wysyłania

  tcp::socket socket;
  bool active;                // czy połączenie jest aktywne

  const std::vector<PrintServer>& servers_table;  // referencja do tabelki
  int table_position;         // aktualna pozycja wyświetlanej tabelki
};

#endif  // TELNET_CONNECTION_H