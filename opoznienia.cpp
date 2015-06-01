#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>

#include <map>
#include "common.h"
#include "measurement_server.h"
#include "measurement_client.h"

int main(int argc, char **argv) {
  std::shared_ptr<servers_map> servers(new servers_map);

  boost::asio::io_service io_service;
  MeasurementClient measurement_client(io_service, servers);
  io_service.run();

}
