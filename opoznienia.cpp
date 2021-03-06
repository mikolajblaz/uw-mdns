#include <iostream>
#include <thread>
#include <stdexcept>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>
#include <boost/thread/thread.hpp>

#include "common.h"
#include "mdns_server.h"
#include "measurement_server.h"
#include "measurement_client.h"


/* Parsuje argumenty. */
void parse_arguments(int argc, char const *argv[], int& udp_port,
    int& ui_port, int& measurement_interval, int& mdns_interval,
    float&  ui_refresh_interval, bool& broadcast_ssh) {
  for (int arg = 1; arg < argc; ++arg) {
    if (strcmp(argv[arg], "-s") == 0) {
      broadcast_ssh = true;

    } else if (arg == argc - 1) {
       // inne argumenty wymagają liczby, a to jest ostatni
      throw std::invalid_argument("parsing error");

    } else {    // mamy przed sobą 2 argumenty
      if (strcmp(argv[arg], "-v") == 0) {    // float
        ui_refresh_interval = std::stof(argv[arg + 1]);
      } else {          // musimy wczytać wartość typu int
        int value = std::stoi(argv[arg + 1]);
        if (strcmp(argv[arg], "-u") == 0) {
          udp_port = value;
        } else if (strcmp(argv[arg], "-U") == 0) {
          ui_port = value;
        } else if (strcmp(argv[arg], "-t") == 0) {
          measurement_interval = value;
        } else if (strcmp(argv[arg], "-T") == 0) {
          mdns_interval = value;
        } else {
          throw std::invalid_argument("unkown argument type");
        }
      }
      arg++;    // wczytaliśmy 2 argumenty
    }
  }
}


int main(int argc, char const *argv[]) {
  /* Domyślne wartości parametrów: */
  int udp_port = UDP_PORT_DEFAULT;    // port usługi opóźnień (udostępnianie i łączenie)
  int ui_port = UI_PORT_DEFAULT;      // port do udostępniania interfejsu telnet
  int measurement_interval = MEASUREMENT_INTERVAL_DEFAULT;
  int mdns_interval = MDNS_INTERVAL_DEFAULT;
  float ui_refresh_interval = UI_REFRESH_INTERVAL_DEFAULT;
  bool broadcast_ssh = BROADCAST_SSH_DEFAULT;     // czy rozgłaszać _ssh._tcp.local

  try {
    parse_arguments(argc, argv, udp_port, ui_port, measurement_interval,
        mdns_interval, ui_refresh_interval, broadcast_ssh);
  } catch (std::invalid_argument) {
    std::cout << "Error parsing arguments: invalid values types!\n";
    return 1;
  } catch (std::out_of_range) {
    std::cout << "Error parsing arguments: too large values!\n";
    return 1;
  }

  


  /* Tworzymy dwa osobne serwisy: */
  boost::asio::io_service io_service;         // do pomiarów czasu
  boost::asio::io_service io_service_servers; // dla serwerów opóźnień i mDNS


	MdnsServer mdns_server(io_service_servers, broadcast_ssh);
  MeasurementServer measurement_server(io_service_servers);
  MeasurementClient measurement_client(io_service, udp_port, ui_port,
      measurement_interval, mdns_interval, ui_refresh_interval);

  std::thread servers_thread(
      boost::bind(&boost::asio::io_service::run, &io_service_servers));
  io_service.run();
  servers_thread.join();
}
