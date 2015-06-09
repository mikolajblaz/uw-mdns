#ifndef TELNET_CONNECTION_H
#define TELNET_CONNECTION_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common.h"
#include "get_time_usec.h"
#include "print_server.h"

const int MAX_OPTIONS = 6;
const std::string IAC_WILL_SGA ("\377\373\003");
const std::string IAC_WILL_ECHO("\377\373\001");

using boost::asio::ip::tcp;

class TelnetConnection {
public:
  TelnetConnection(boost::asio::io_service& io_service, std::vector<PrintServer> const& servers_table) :
    socket(io_service),
    active(false),
    servers_table(servers_table) {}

  tcp::socket& get_socket() { return socket; }
  bool is_active() const { return active; }

  /* Aktywuje nowo ustanowione połączenie. */
  void activate() {
    active = true;
    negotiate_options();
  }
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
  void send_update(float max_delay) {
    last_max_delay = max_delay;
    send_buffer.clear();
    send_buffer.reserve(UI_SCREEN_HEIGHT);
    for (int i = table_position; i < servers_table.size() && i < table_position + UI_SCREEN_HEIGHT; ++i) {
      send_buffer.push_back(servers_table[i].to_string_sec(max_delay));
    }

    boost::asio::async_write(socket, boost::asio::buffer(send_buffer),
        boost::bind(&TelnetConnection::handle_send, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
  }

private:
  void handle_send(boost::system::error_code const& error,
      std::size_t bytes_transferred) {
    if (error)
      throw boost::system::system_error(error);
    std::cout << "TELNET with " << socket.remote_endpoint().address() << ": send update finished successfully, sent " << bytes_transferred << " bytes!\n";
  }

  /* Obsługa odebranych danych od klienta. */
  void handle_receive(boost::system::error_code const& error,
      std::size_t bytes_transferred) {
    if (error) {
      deactivate();   // koniec połączenia
    } else {
      std::cout << "TELNET with " << socket.remote_endpoint().address() << ": received " << bytes_transferred << " bytes:[";
      std::cout.write(recv_buffer.data(), bytes_transferred);
      std::cout << "]\n";

      start_receive();
    }
  }

  /* Funkcja negocjująca odpowiednie opcje z klientem telnet. */
  void negotiate_options() {
    send_options(IAC_WILL_SGA + IAC_WILL_ECHO);
  }
  /* Wysłanie opcji 'options' do klienta telnetu. */
  void send_options(std::string const& options) {
    boost::system::error_code error;
    socket.send(boost::asio::buffer(options), 0, error);
    if (error)
      deactivate();     // TODO może bez tego?
  }
  /* Oczekiwanie na potwierdzenie opcji */
  void recv_options(std::string const& required_options) {
    std::shared_ptr<boost::array<char, MAX_OPTIONS> > buffer(new boost::array<char, MAX_OPTIONS>);
    boost::asio::async_read(socket, boost::asio::buffer(*buffer),
        boost::bind(&TelnetConnection::handle_recv_options, this, buffer, required_options,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
  }
  /* Sprawdzenie zgodności odpowiedzi z wymaganiami. */
  void handle_recv_options(std::shared_ptr<boost::array<char, MAX_OPTIONS> > buffer,
      std::string const& required_options, boost::system::error_code const& error,
      std::size_t bytes_transferred) {
    if (error || std::string(buffer->begin(), buffer->end()) != required_options) {
      deactivate();   // nieporozumienie w negocjacjach
    } else {
      negotiate_options();  // OK, można kontynuować
    }
  }


  boost::array<char, BUFFER_SIZE> recv_buffer;
  tcp::socket socket;
  bool active;                // czy połączenie jest aktywne

  const std::vector<PrintServer>& servers_table;
  std::vector<std::string> send_buffer;
  int table_position;         // aktualna pozycja wyświetlanej tabelki
  float last_max_delay;       // ostatnio zarejestrowane największe opóźnienie
};

#endif  // TELNET_CONNECTION_H