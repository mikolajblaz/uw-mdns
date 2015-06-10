#ifndef PRINT_SERVER_H
#define PRINT_SERVER_H

#include <sstream>
#include "common.h"
#include "server.h"

/* Klasa zawierająca napis, który wyswietlany jest klientowi telnetu. */
class PrintServer {
public:
  PrintServer(Server const& server, float max_delay) :
      average_delay(0), to_print(construct_string(server, max_delay)) {}

  /* Zwraca napis długości 80 z rozmieszeniem opóźnień (w sekundach!)
   * proporcjonalnym do średniego opóźnienia (względem opóźnienia 'max_delay'). */
  std::string construct_string(Server const& server, float max_delay) {
    std::ostringstream numbers_stream;
    float delay;          // opóźnienie w sekundach
    int proto_cnt = 0;    // liczba protokołów uwzględnianych do średniej
    int last_char;

    /* Konstruujemy liczby oznaczające kolejne opóźnienia: */
    for (int proto = PROTOCOL::UDP; proto < PROTOCOL_COUNT; proto++) {
      if (server.finished[proto].empty()) {
        numbers_stream << " ---";
      } else {
        delay = server.delays_sum[proto] / server.finished[proto].size() / SEC_TO_USEC;
        average_delay += delay;
        proto_cnt++;
        numbers_stream << ' ' << delay;
      }
    }
    average_delay = proto_cnt ? average_delay / proto_cnt : 0;

    std::string numbers(numbers_stream.str());
    std::string ip(server.ip->to_v4().to_string());
    ip = ip + std::string(IP_WIDTH - ip.size(), ' ');   // wyrównanie IP

    /* zwykłe wypisanie: */
    if (max_delay == 0)
      return ip + numbers + std::string(UI_SCREEN_WIDTH - ip.size() - numbers.size(), ' ');

    /* else obliczamy odpowiednią pozycję ostatniego drukowalnego znaku */
    last_char = std::ceil(average_delay / max_delay * UI_SCREEN_WIDTH);
    last_char = std::max(last_char, (int) (numbers.size() + ip.size()));
    last_char = std::min(last_char, UI_SCREEN_WIDTH);

    /* Wynik to ip oraz liczby oznaczające opóźnienia z odpowiednią liczbą
     * spacji przed i po liczbach: */
    return ip +
        std::string(last_char - numbers.size() - ip.size(), ' ') +
        numbers +
        std::string(UI_SCREEN_WIDTH - last_char, ' ');
  }

  /* Sortowanie po średnim opóźnieniu malejąco, potem po adresach IP leksykograficznie. */
  friend bool operator<(PrintServer const& ps1, PrintServer const& ps2) {
    if (ps1.average_delay == ps2.average_delay) {
      return ps1.to_print < ps2.to_print;
    } else {
      return ps1.average_delay > ps2.average_delay;   // malejąco!
    }
  }

  /* Wypisywanie stringa: */
  friend std::ostream& operator<<(std::ostream& os, PrintServer const& ps) {
    return os << ps.to_print;
  }

private:
  float average_delay;
  std::string to_print;
};

#endif  // PRINT_SERVER_H