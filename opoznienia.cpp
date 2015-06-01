#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>

#include <map>
#include "common.h"
#include "server.h"
#include "mdns_server.h"
#include "mdns_client.h"
#include "measurement_server.h"
#include "measurement_client.h"

int main(int argc, char const *argv[]) {
  servers_ptr servers(new servers_map);
  boost::asio::io_service io_service;

  MdnsServer mdns_server(io_service);
  //MdnsClient mdns_client(io_service, servers);
  //MeasurementServer measurement_server(io_service);
  MeasurementClient measurement_client(io_service, servers);

  io_service.run();

}
