#include <iostream>
#include <thread>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>
#include <boost/thread/thread.hpp>

#include "common.h"
#include "mdns_server.h"
#include "measurement_server.h"
#include "measurement_client.h"

int main(int argc, char const *argv[]) {
  /* Tworzymy dwa osobne serwisy: */
  boost::asio::io_service io_service;         // do pomiarów czasu
  boost::asio::io_service io_service_servers; // dla serwerów opóźnień i mDNS

  try {
    MdnsServer mdns_server(io_service_servers);
  } catch (boost::system::system_error e) {
    std::cerr << "Failed to start mDNS Server!";
  }

  try {
    MeasurementServer measurement_server(io_service_servers);
  } catch (boost::system::system_error e) {
    std::cerr << "Failed to start Measurement Server!";
  }
  try {
    MeasurementClient measurement_client(io_service);
  } catch (boost::system::system_error e) {
    std::cerr << "Failed to start Measurement Client!";
  }

  std::thread servers_thread(
      boost::bind(&boost::asio::io_service::run, &io_service_servers));
  io_service.run();
  servers_thread.join();
}
