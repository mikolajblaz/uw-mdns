#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>

#include <map>
#include "common.h"
#include "mdns_message.h"
#include "server.h"
#include "mdns_server.h"
#include "mdns_client.h"
#include "measurement_server.h"
#include "measurement_client.h"
#include "telnet_server.h"

int main(int argc, char const *argv[]) {
  servers_ptr servers(new servers_map);
  boost::asio::io_service io_service;
  //boost::asio::io_service io_service_servers;  // TODO

  //try {
  	//MdnsServer mdns_server(io_service);
    //MdnsClient mdns_client(io_service, servers);
    //MeasurementServer measurement_server(io_service);
    //MeasurementClient measurement_client(io_service, servers);
    TelnetServer telnet_server(io_service, servers);

    io_service.run();

  /*} catch (boost::system::system_error e) {               // TODO
    //std::cerr << "Failed to start mDNS server: port " << MDNS_PORT_DEFAULT << " already in use!\n";
    std::cerr << "ERROR: " << boost::diagnostic_information(e) << std::endl;
  }
/**/
}
