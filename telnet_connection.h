#ifndef TELNET_CONNECTION_H
#define TELNET_CONNECTION_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common.h"
#include "get_time_usec.h"

const int MAX_OPTIONS = 3;

const std::string IAC_WILL_SGA ("\377\373\003");
const std::string IAC_DO_SGA   ("\377\375\003");
const std::string IAC_WILL_ECHO("\377\373\001");
const std::string IAC_DO_ECHO  ("\377\375\001");

const std::string IAC ("\377");
const std::string DONT("\376");
const std::string DO  ("\375");
const std::string WONT("\374");
const std::string WILL("\373");
const std::string SGA ("\003"); //TODO chyba lepiej char

enum class STATE {
  INIT, WILL_SGA, WILL_ECHO, READY
};



//     write (connfd, IAC, 1);
//     write (connfd, WILL, 1);
//     write (connfd, WILL_SGA, 1);
//     recv (connfd, bafer, 3, MSG_WAITALL);
//     int i;
//     for (i=0;i<strlen(bafer);i++)
//         switch ((unsigned char)bafer[i]) {
//             case 255: printf("IAC\n");break;
//             case 254: printf("DONT\n");break;
//             case 253: printf("DO\n");break;
//             case 252: printf("WONT\n");break;
//             case 251: printf("WILL\n");break;
//             case 3: printf("SUPPRESS-GO-AHEAD\n");break;
//             case 1: printf("WILL_ECHO\n");break;
//         }

//     write (connfd, IAC, 1);
//     write (connfd, WILL, 1);
//     write (connfd, WILL_ECHO, 1);
//     recv (connfd, bafer, 3, MSG_WAITALL);
//     for (i=0;i<strlen(bafer);i++)
//         switch ((unsigned char)bafer[i]) {
//             case 255: printf("IAC\n");break;
//             case 254: printf("DONT\n");break;
//             case 253: printf("DO\n");break;
//             case 252: printf("WONT\n");break;
//             case 251: printf("WILL\n");break;
//             case 3: printf("SUPPRESS-GO-AHEAD\n");break;
//             case 1: printf("WILL_ECHO\n");break;
//         }

//     write(connfd, CLR_SCR, 7);
//     write(connfd, RED_LETTERS, 7);
//     write (connfd, "Enter key: ", 11);

// do {
//     recv (connfd,bafer,1,MSG_WAITALL);
//     write(connfd, bafer, 1);
//     printf("%d\n",(unsigned char)bafer[0]);
// } while (bafer[0]!='I');
//     write (connfd, "\n",1);
//     write(connfd, "\033[0m", strlen("\033[0m")); // reset color
//     close(connfd);
//  }



using boost::asio::ip::tcp;

class TelnetConnection {
public:
  TelnetConnection(boost::asio::io_service& io_service) :
    socket(io_service),
    active(false),
    state(STATE::INIT) {
    std::cout << "New TelnetConnection!!!" << std::endl; // TODO remove
  }

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
  void send_update(boost::array<char, UI_SCREEN_WIDTH> const& send_buffer) {
    if (state == STATE::READY)
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

  void negotiate_options() {
    switch (state) {
      case STATE::INIT:
        state = STATE::WILL_ECHO;   // TODO omijanie ECHO
        send_options(IAC_WILL_SGA);
        recv_options(IAC_DO_SGA);
        break;
      case STATE::WILL_SGA:
        state = STATE::WILL_ECHO;
        send_options(IAC_WILL_ECHO);
        recv_options(IAC_DO_ECHO);
        break;
      case STATE::WILL_ECHO:
        state = STATE::READY;
        start_receive();
        break;
      case STATE::READY:
        break;
    }
  }

  void send_options(std::string const& options) {
    boost::system::error_code error;
    tcp::socket::message_flags flags;
    socket.send(boost::asio::buffer(options), flags, error);
    if (error)
      deactivate();
  }
  void recv_options(std::string const& required_options) {
    std::shared_ptr<boost::array<char, MAX_OPTIONS> > buffer(new boost::array<char, MAX_OPTIONS>);
    boost::asio::async_read(socket, boost::asio::buffer(*buffer),
        boost::bind(&TelnetConnection::handle_recv_options, this, buffer, required_options,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
  }

  void handle_recv_options(std::shared_ptr<boost::array<char, MAX_OPTIONS> > buffer,
      std::string const& required_options, boost::system::error_code const& error,
      std::size_t bytes_transferred) {
    if (error || std::string(buffer->begin(), buffer->end()) != required_options) {
      deactivate();   // koniec połączenia
    } else {          // OK
      negotiate_options();
    }
  }


  boost::array<char, BUFFER_SIZE> recv_buffer;
  tcp::socket socket;
  bool active;                // czy połączenie jest aktywne
  STATE state;

  int table_position;         // aktualna pozycja wyświetlanej tabelki
};

#endif  // TELNET_CONNECTION_H