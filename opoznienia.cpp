#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>
#include <boost/exception/diagnostic_information.hpp>     // TODO

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

  //try {
  	MdnsServer mdns_server(io_service);
    MdnsClient mdns_client(io_service, servers);
    //MeasurementServer measurement_server(io_service);
    //MeasurementClient measurement_client(io_service, servers);

    io_service.run();

  /*} catch (boost::system::system_error e) {               // TODO
    //std::cerr << "Failed to start mDNS server: port " << MDNS_PORT_DEFAULT << " already in use!\n";
    std::cerr << "ERROR: " << boost::diagnostic_information(e) << std::endl;
  }
/**/
}
