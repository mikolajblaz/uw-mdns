#ifndef MDNS_SERVER_H
#define MDNS_SERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include "common.h"
#include "mdns_message.h"

using boost::asio::ip::udp;
using boost::asio::ip::address;

class MdnsServer {
public:
  MdnsServer(boost::asio::io_service& io_service) :
      multicast_endpoint(address::from_string(MDNS_ADDRESS), MDNS_PORT),
      send_socket(io_service, multicast_endpoint.protocol()),
      recv_socket(io_service),
      local_server_address(get_local_server_address()),
      local_server_name(),
      opoznienia_service(OPOZNIENIA_SERVICE),
      ssh_service(SSH_SERVICE) {
    /* dołączamy do grupy adresu 224.0.0.251, odbieramy na porcie 5353: */
    recv_socket.open(udp::v4());
    recv_socket.set_option(udp::socket::reuse_address(true));
    recv_socket.bind(udp::endpoint(udp::v4(), MDNS_PORT));    // port 5353
    recv_socket.set_option(boost::asio::ip::multicast::join_group(
        address::from_string(MDNS_ADDRESS)));     // adres 224.0.0.251

    /* nie pozwalamy na wysyłanie do siebie: */
    boost::asio::ip::multicast::enable_loopback option(false);
    //send_socket.set_option(option);     // TODO turn on?

    // TODO connect
    local_server_name = get_local_server_name();

    start_receive();
  }

private:
  /* Zwraca nową nazwę serwera w sieci lokalnej. */
  std::string get_local_server_name() {
    return boost::asio::ip::host_name() + '.' + OPOZNIENIA_SERVICE;
  }
  /* Zwraca adres IP serwera w sieci lokalnej. */
  uint32_t get_local_server_address() {
    // TODO wziąć z podłączonego gniazda UDP
    return address::from_string("192.168.0.15").to_v4().to_ulong();
  }

  /* zlecenie asynchronicznego odbioru pakietów multicastowych */
  void start_receive() {
    recv_stream_buffer.consume(recv_stream_buffer.size());  // wyczyść bufor

    recv_socket.async_receive_from(
        recv_stream_buffer.prepare(BUFFER_SIZE), remote_endpoint,
        boost::bind(&MdnsServer::handle_receive, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

  /* Próbujemy przeczytać pakiet mDNS typu 'Query'. Zapytanie typu 'Response'
   * jest ignorowane, zaś niepoprawne zapytanie typu 'Query' powoduje rzucenie
   * (i złapanie) wyjątku.
   * Następnie serwer odpowiada pakietem mDNS typu 'Response'. */
  void handle_receive(boost::system::error_code const& error,
      std::size_t bytes_transferred) {
    if (error)
      throw boost::system::system_error(error);

    recv_stream_buffer.commit(bytes_transferred);   // przygotowanie bufora
    std::istream is(&recv_stream_buffer);
    MdnsQuery query;

    try {
      if (query.try_read(is)) {          // ignorujemy pakiety mDNS typu 'Response'
        std::cout << "mDNS SERVER: datagram received: [" << query << "] from: " << remote_endpoint << "\n";

        /* jeśli znamy jakąś odpowiedź: */
        MdnsResponse response = respond_to(query);
        if (!response.get_answers().empty()) {
          boost::asio::streambuf request_buffer;
          std::ostream os(&request_buffer);
          os << response;

          send_socket.async_send_to(request_buffer.data(), multicast_endpoint,
              boost::bind(&MdnsServer::handle_send, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
        }
      }
    } catch (InvalidMdnsMessageException e) {
      std::cout << "mDNS SERVER: Ignoring packet... reason: " << e.what() << std::endl;
    }

    start_receive();
  }

  void handle_send(boost::system::error_code const& error,
      std::size_t /*bytes_transferred*/) {
    if (error)
      throw boost::system::system_error(error);
    std::cout << "mDNS SERVER: Sent DNS response!\n";
  }


  MdnsResponse respond_to(MdnsQuery const& query) {
    MdnsResponse response;
    std::vector<MdnsQuestion> questions = query.get_questions();
    for (int i = 0; i < questions.size(); i++) {
      try {
        response.add_answer(answer_to(questions[i]));
      }
      catch (InvalidMdnsMessageException e) {}    // ignore unknown questions
    }
    return response;
  }

  MdnsAnswer answer_to(MdnsQuestion const& question) {
    MdnsDomainName name = question.get_name();
    uint16_t type = question.get_qtype();
    uint16_t _class = INTERNET_CLASS;
    uint32_t ttl = TTL_DEFAULT;
    if (type == static_cast<uint16_t>(QTYPE::PTR) && (name == opoznienia_service || name == ssh_service)) {
      return MdnsAnswer(name, type, _class, ttl, local_server_name);
    } else if (type == static_cast<uint16_t>(QTYPE::A) && name == local_server_name) {
      return MdnsAnswer(name, type, _class, ttl, local_server_address);
    } else {
      throw InvalidMdnsMessageException("Unknown question type");
    }
  }

  boost::array<char, BUFFER_SIZE> recv_buffer;
  boost::asio::streambuf recv_stream_buffer;

  udp::endpoint multicast_endpoint;   // odbieranie na porcie 5353 z adresu 224.0.0.251
  udp::endpoint remote_endpoint;      // endpoint nadawcy odbieranego pakietu
  udp::socket send_socket;            // wysyłanie pakietów na multicast
  udp::socket recv_socket;            // odbieranie z multicastowych

  uint32_t local_server_address;      // rozgłaszane IP serwera
  MdnsDomainName local_server_name;   // rozgłaszana nazwa serwera
  const MdnsDomainName opoznienia_service;
  const MdnsDomainName ssh_service;
};

#endif  // MDNS_SERVER_H