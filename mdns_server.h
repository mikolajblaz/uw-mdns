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
  MdnsServer(boost::asio::io_service& io_service, bool broadcast_ssh) :
      recv_buffer(),
      send_buffer(),
      recv_stream(&recv_buffer),
      send_stream(&send_buffer),
      multicast_endpoint(address::from_string(MDNS_ADDRESS), MDNS_PORT),
      send_socket(io_service, multicast_endpoint.protocol()),
      recv_socket(io_service),
      local_opoznienia_name(get_local_opoznienia_name()),
      local_ssh_name(get_local_ssh_name()),
      opoznienia_service(OPOZNIENIA_SERVICE),
      ssh_service(SSH_SERVICE),
      broadcast_ssh(broadcast_ssh) {
    try {
      /* dołączamy do grupy adresu 224.0.0.251, odbieramy na porcie 5353: */
      recv_socket.open(udp::v4());
      recv_socket.set_option(udp::socket::reuse_address(true));
      recv_socket.bind(udp::endpoint(udp::v4(), MDNS_PORT));    // port 5353
      recv_socket.set_option(boost::asio::ip::multicast::join_group(
          address::from_string(MDNS_ADDRESS)));     // adres 224.0.0.251

      /* nie pozwalamy na wysyłanie do siebie: */
      boost::asio::ip::multicast::enable_loopback option(false);
      //send_socket.set_option(option);     // TODO turn on?

      /* łączymy się z adresem multicastowym do wysyłania: */
      send_socket.connect(multicast_endpoint);
      local_server_address = get_local_server_address();

      start_receive();

    } catch (boost::system::error_code ec) {
      std::cerr << "Failed to start mDNS Server!\n";
    }
  }

private:
  /* Zwraca nową nazwę serwera usługi opóźnień w sieci lokalnej. */
  std::string get_local_opoznienia_name() {
    return boost::asio::ip::host_name() + '.' + OPOZNIENIA_SERVICE;
  }
  /* Zwraca nową nazwę serwera usługi ssh w sieci lokalnej. */
  std::string get_local_ssh_name() {
    return boost::asio::ip::host_name() + '.' + SSH_SERVICE;
  }
  /* Zwraca adres IP serwera w sieci lokalnej. */
  uint32_t get_local_server_address() {
    return send_socket.local_endpoint().address().to_v4().to_ulong();
  }

  /* zlecenie asynchronicznego odbioru pakietów multicastowych */
  void start_receive() {
    recv_buffer.consume(recv_buffer.size());  // wyczyść bufor

    recv_socket.async_receive_from(
        recv_buffer.prepare(BUFFER_SIZE), remote_endpoint,
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

    MdnsQuery query;
    recv_buffer.commit(bytes_transferred);   // przygotowanie bufora

    try {
      if (query.try_read(recv_stream)) {          // ignorujemy pakiety mDNS typu 'Response'
        std::cout << "mDNS SERVER: datagram received: [" << query << "] from: " << remote_endpoint << "\n";

        send_response_to(query);
      }
    } catch (InvalidMdnsMessageException e) {
      std::cout << "mDNS SERVER: Ignoring packet... reason: " << e.what() << std::endl;
    }

    start_receive();
  }

  /* Tworzy i wysyła odpowiedź na zapytanie 'query' */
  void send_response_to(MdnsQuery const& query) {
    MdnsResponse response;
    std::vector<MdnsQuestion> questions = query.get_questions();
    for (int i = 0; i < questions.size(); i++) {
      try {
        response.add_answer(answer_to(questions[i]));
      }
      catch (InvalidMdnsMessageException e) {}    // ignorujemy nieznane pytania
    }

    /* jeśli znamy jakąś odpowiedź, odpowiadamy: */
    if (!response.get_answers().empty()) {
      send_buffer.consume(send_buffer.size());
      send_stream << response;

      send_socket.async_send_to(send_buffer.data(), multicast_endpoint,
          boost::bind(&MdnsServer::handle_send, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }
  }

  /* Zwraca odpowiedź na pojedyncze pytanie mDNS. */
  MdnsAnswer answer_to(MdnsQuestion const& question) {
    MdnsDomainName name = question.get_name();
    uint16_t type = question.get_qtype();
    uint16_t _class = INTERNET_CLASS;
    uint32_t ttl = TTL_DEFAULT;
    if (type == static_cast<uint16_t>(QTYPE::PTR)) {
      if (name == opoznienia_service)
        return MdnsAnswer(name, type, _class, ttl, local_opoznienia_name);
      if (name == ssh_service && broadcast_ssh)   // tylko jeśli rozgłaszamy ssh
        return MdnsAnswer(name, type, _class, ttl, local_ssh_name);

    } else if (type == static_cast<uint16_t>(QTYPE::A)) {
      if (name == local_opoznienia_name || (name == local_ssh_name && broadcast_ssh))
        return MdnsAnswer(name, type, _class, ttl, local_server_address);
    }

    throw InvalidMdnsMessageException("Unknown question type");
  }

  void handle_send(boost::system::error_code const& error,
      std::size_t /*bytes_transferred*/) {
    if (error)
      throw boost::system::system_error(error);
    std::cout << "mDNS SERVER: Sent DNS response!\n";
  }



  boost::asio::streambuf recv_buffer; // bufor do odbierania
  boost::asio::streambuf send_buffer; // bufor do wysyłania
  std::istream recv_stream;           // strumień do odbierania
  std::ostream send_stream;           // strumień do wysyłania

  udp::endpoint multicast_endpoint;   // odbieranie na porcie 5353 z adresu 224.0.0.251
  udp::endpoint remote_endpoint;      // endpoint nadawcy odbieranego pakietu
  udp::socket send_socket;            // wysyłanie pakietów na multicast
  udp::socket recv_socket;            // odbieranie z multicastowych

  uint32_t local_server_address;              // rozgłaszane IP serwera
  const MdnsDomainName local_opoznienia_name; // rozgłaszana nazwa serwera usługi opoznienia
  const MdnsDomainName local_ssh_name;        // rozgłaszana nazwa serwera usługi ssh
  const MdnsDomainName opoznienia_service;
  const MdnsDomainName ssh_service;

  bool broadcast_ssh;
};

#endif  // MDNS_SERVER_H