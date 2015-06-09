#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>

#include <map>
#include "common.h"
#include "mdns_server.h"
#include "measurement_server.h"
#include "measurement_client.h"

int main(int argc, char const *argv[]) {
  /* Tworzymy dwa osobne serwisy: */
  boost::asio::io_service io_service;         // do pomiarów czasu
  boost::asio::io_service io_service_servers; // dla serwerów opóźnień i mDNS


	MdnsServer mdns_server(io_service_servers);
  MeasurementServer measurement_server(io_service_servers);
  MeasurementClient measurement_client(io_service);

  boost::thread servers_thread(
      boost::bind(&boost::asio::io_service::run, &io_service_servers));
  io_service.run();
  servers_thread.join();
}
