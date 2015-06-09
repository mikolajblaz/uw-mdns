#ifndef PRINT_SERVER_H
#define PRINT_SERVER_H

#include <sstream>
#include "common.h"
#include "server.h"

class PrintServer {
public:
  PrintServer(Server const& server) : ip(server.ip->to_v4().to_string()), average_delay(0) {
    for (int proto = PROTOCOL::UDP; proto < PROTOCOL_COUNT; proto++) {
      delay[proto] = server.finished[proto].empty() ? 0 : server.delays_sum[proto] / server.finished[proto].size();
      average_delay += delay[proto];
    }
    average_delay /= PROTOCOL_COUNT;
  }

  PrintServer(PrintServer const& ps) : ip(ps.ip), average_delay(ps.average_delay) {
    for (int proto = PROTOCOL::UDP; proto < PROTOCOL_COUNT; proto++)
      delay[proto] = ps.delay[proto];
  }

  PrintServer& operator=(PrintServer const& ps) {
    ip = ps.ip;
    average_delay = ps.average_delay;
    for (int proto = PROTOCOL::UDP; proto < PROTOCOL_COUNT; proto++)
      delay[proto] = ps.delay[proto];
    return *this;
  }

  float delay_sec() { return average_delay / SEC_TO_USEC; }

  /* Zwraca napis długości 80 z rozmieszeniem opóźnień (w sekundach!)
   * proporcjonalnym do średniego opóźnienia (względem opóźnienia 'max_delay'). */
  std::string to_string_sec(float max_delay) const {
    std::ostringstream stream;
    for (int proto = PROTOCOL::UDP; proto < PROTOCOL_COUNT; proto++) {
      stream << ' ' << (float) delay[proto] / SEC_TO_USEC;
    }
    std::string numbers = stream.str();
    /* Pozycja ostatniego znaku: */
    int last_char = max_delay == 0 ? UI_SCREEN_WIDTH :
        std::ceil(UI_SCREEN_WIDTH * average_delay / SEC_TO_USEC / max_delay);

    /* Wstawia odpowiednią liczbę spacji przed i po liczbach: */
    return ip + std::string(last_char - numbers.size() - ip.size(), ' ') +
        numbers + std::string(UI_SCREEN_WIDTH - last_char, ' ');
  }

  /* Sortowanie po średnim opóźnieniu, potem po adresach IP leksykograficznie. */
  friend bool operator<(PrintServer const& ps1, PrintServer const& ps2) {
    if (ps1.average_delay == ps2.average_delay) {
      return ps1.ip < ps2.ip;
    } else {
      return ps1.average_delay < ps2.average_delay;
    }
  }

private:
  std::string ip;
  time_type delay[PROTOCOL_COUNT];
  time_type average_delay;
};

#endif  // PRINT_SERVER_H