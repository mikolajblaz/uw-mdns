#ifndef MDNS_MESSAGE_H
#define MDNS_MESSAGE_H

enum class QTYPE : uint16_t {
  A = 1,
  PTR = 12
};

const uint16_t INTERNET_CLASS = 0x0001;
const int MAX_DOMAINS_DEPTH = 10;    // maksymalna dpouszczalna głębokość drzewa domenowego
const int MAX_DOMAIN_LENGTH = 255;   // maksymalna długość nazwy domeny w bajtach

/* Wypisuje 'val' na strumień 'os' w formacie big endian. */
inline std::ostream& write_be(std::ostream& os, uint16_t val) {
  unsigned char c[2];
  c[0] = static_cast<unsigned char>(val >> 8);
  c[1] = static_cast<unsigned char>(val);
  return os << c[0] << c[1];
}

/* Czyta 'val' ze strumienia 'is' zapisane w formacie big endian. */
inline std::istream& read_be(std::istream& is, uint16_t& val) {
  unsigned char c[2];
  is >> c[0] >> c[1];
  val = (c[0] << 8) + c[1];
  return is;
}


/* Klasa reprezentująca nagłówek mDNS. */
class MdnsHeader {
public:
  MdnsHeader() {
    std::fill(data, data + sizeof(data), 0);
  }

  /* gettery flag: */
  bool qr() const { return data[2] & 0x80; }     // query/response flag
  bool opcode() const { return data[3] & 0x78; } // purpose of message     // TODO czy niepuste
  bool aa() const { return data[3] & 0x40; }     // authoritive answer  
  bool tc() const { return data[3] & 0x20; }     // truncated message
  // pole RD, RA, Z, AD, CD jest ignorowane
  bool rcode() const { return data[4] & 0x0F; }  // response code           // TODO

  // /* settery: */
  void qr() { data[2] = data[2] | 0x80; }     // query/response flag
  // pole OPCODE musi zawierać 0
  void aa() { data[3] = data[3] | 0x40; }     // authoritive answer  
  void tc() { data[3] = data[3] | 0x20; }     // truncated message
  // pole RD, RA, Z, AD, CD, RCODE musi zawierać 0

  uint16_t id()          { return (data[0] << 8) + data[1]; }
  uint16_t q_count()     { return (data[4] << 8) + data[5]; }
  uint16_t ans_count()   { return (data[6] << 8) + data[7]; }
  uint16_t auth_count()  { return (data[8] << 8) + data[9]; }
  uint16_t add_count()   { return (data[10] << 8) + data[11]; }

  void id(uint16_t val)          { data[0] = val & 0x00FF; data[1] = val >> 8; }
  void q_count(uint16_t val)     { data[4] = val & 0x00FF; data[5] = val >> 8; }
  void ans_count(uint16_t val)   { data[6] = val & 0x00FF; data[7] = val >> 8; }
  void auth_count(uint16_t val)  { data[8] = val & 0x00FF; data[9] = val >> 8; }
  void add_count(uint16_t val)   { data[10] = val & 0x00FF; data[11] = val >> 8; }

  friend std::istream& operator>>(std::istream& is, MdnsHeader& header) {
    return is.read(reinterpret_cast<char*>(header.data), header_length);
  }

  friend std::ostream& operator<<(std::ostream& os, MdnsHeader const& header) {
    return os.write(reinterpret_cast<const char*>(header.data), header_length);
  }

private:
  static const std::streamsize header_length = 12;    // dł. nagłówka w bajtach;
  unsigned char data[header_length];             // dane nagłówka
};  // class MdnsHeader


/* Klasa reprezentująca pełną nazwę domenową. */
class MdnsDomainName {
public:
  MdnsDomainName() {}
  MdnsDomainName(std::string name_str) {
    std::stringstream str(name_str);
    std::string part;
    while (std::getline(str, part, '.')) {
      data.push_back(std::string(part));
    }
  }

  uint16_t size() {
    uint16_t result;
    for (int i = 0; i < data.size(); i++) {
      result += data[i].size() + 1;   // + 1 to bajt zliczający długość
    }
    return result;
  }

  friend std::istream& operator>>(std::istream& is, MdnsDomainName& domain_name) {
    unsigned char next_length;
    char buffer[MAX_DOMAIN_LENGTH];

    /* wczytujemy kolejne nazwy domen. */
    is >> next_length;
    while (next_length != 0 && domain_name.data.size() <= MAX_DOMAINS_DEPTH) {
      is.read(buffer, next_length);
      domain_name.data.push_back(std::string(buffer, next_length));    // TODO czy działa
      is >> next_length;
    }
    if (domain_name.data.size() > MAX_DOMAINS_DEPTH) {
      throw boost::system::system_error(boost::system::error_code());    // TODO ????????
    }
    
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, MdnsDomainName const& domain_name) {
    for (int i = 0; i < domain_name.data.size(); i++) {
      os << static_cast<unsigned char>(domain_name.data[i].size());
      os.write(domain_name.data[i].c_str(), domain_name.data[i].size());
    }
    os.write("\0", 1);
    return os;
  }

private:
  std::vector<std::string> data;
};  // class MdnsDomainName


/* Klasa reprezentująca pojedyncze zapytanie mDNS. */
class MdnsQuestion {
public:
  MdnsQuestion() {}
  MdnsQuestion(std::string const& domain_name, uint16_t qtype, uint16_t qclass = INTERNET_CLASS) :
      name(domain_name), qtype(qtype), qclass(qclass) {}

  friend std::istream& operator>>(std::istream& is, MdnsQuestion& question) {
    is >> question.name;
    read_be(is, question.qtype);
    read_be(is, question.qclass);
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, MdnsQuestion const& question) {
    os << question.name;
    write_be(os, question.qtype);
    write_be(os, question.qclass);
    return os;
  }

private:
  MdnsDomainName name;
  uint16_t qtype;
  uint16_t qclass;
};  // class MdnsQuestion


/* Klasa reprezentująca cały pakiet mDNS (wraz z nagłówkiem). */
class MdnsQuery {
public:
  /* konstruktor tworzy nagłówek mDNS i ustawia jego flagi na 0x0000. */
  MdnsQuery() : header() {}

  void add_question(std::string const& domain_name, QTYPE type) {
    header.q_count(header.q_count() + 1);   // zwiększa licznik pytań w nagłówku  // TODO ładniej/efektywniej
    questions.push_back(MdnsQuestion(domain_name, static_cast<uint16_t>(type)));
  }

  friend std::istream& operator>>(std::istream& is, MdnsQuery& query) {
    is >> query.header;
    for (int i = 0; i < query.header.q_count(); i++) {
      MdnsQuestion question;
      is >> question;
      query.questions.push_back(std::move(question));
    }
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, MdnsQuery const& query) {
    os << query.header;
    for (int i = 0; i < query.questions.size(); i++) {
      os << query.questions[i];
    }
    return os;
  }

private:
  MdnsHeader header;
  std::vector<MdnsQuestion> questions;
};  // class MdnsQuery


/* Klasa reprezentująca pojedynczy Resource Record. W zależności od jego typu
   (PTR lub A) używana jest zmienna server_address lub server_name.
   Nie użyto narzucającego się dziedziczenia klas ze względu na prostotę
   tego rozwiązania. */
class MdnsResourceRecord {
public:
  MdnsResourceRecord() {}
  /* rekord typu "PTR" - nieużywany adres */
  MdnsResourceRecord(uint16_t type, uint16_t _class, uint32_t ttl, std::string const& server_name) :
      type(type), _class(_class), ttl(ttl), rr_len(server_name.size()), server_name(server_name) {
        if (type != static_cast<uint16_t>(QTYPE::PTR))
          ;// TODO throw !
      }
  /* rekord typu "A" - nieużywana nazwa */
  MdnsResourceRecord(uint16_t type, uint16_t _class, uint32_t ttl, uint32_t server_address) :
      type(type), _class(_class), ttl(ttl), rr_len(sizeof(server_address)), server_address(server_address) {
        if (type != static_cast<uint16_t>(QTYPE::A))
          ;// TODO throw !
      }

  friend std::istream& operator>>(std::istream& is, MdnsResourceRecord& rr) {
    read_be(is, rr.type);
    read_be(is, rr._class);
    //read_be(is, rr.ttl);
    read_be(is, rr.rr_len);
    switch (rr.type) {
      case static_cast<uint16_t>(QTYPE::PTR): is >> rr.server_name; break;
      //case static_cast<uint16_t>(QTYPE::A): read_be(is, server_address); break;
      default: throw boost::system::system_error(boost::system::error_code()); // TODO throw!
    }
    if (rr.server_name.size() != rr.rr_len)
      throw boost::system::system_error(boost::system::error_code()); // TODO throw!
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, MdnsResourceRecord const& rr) {
    write_be(os, rr.type);
    write_be(os, rr._class);
    //write_be(os, rr.ttl);
    write_be(os, rr.rr_len);
    switch (rr.type) {
      case static_cast<uint16_t>(QTYPE::PTR): os << rr.server_name; break;
      //case static_cast<uint16_t>(QTYPE::A): write_be(os, server_address); break;
      default: throw boost::system::system_error(boost::system::error_code()); // TODO throw!
    }
    return os;
  }

private:
  uint16_t type;                // typ rekordu
  uint16_t _class;
  uint32_t ttl;
  uint16_t rr_len;
  /* W zależności od rodzaju zapytania używany jest jedna z dwóch zmiennych: */
  MdnsDomainName server_name;   // dla rekordu typu "PTR"
  uint32_t server_address;      // dla rekordu typu "A"
};

/* Klasa reprezentująca pojedynczą odpowiedź. */
class MdnsAnswer {
public:
  MdnsAnswer() {}
  MdnsAnswer(std::string const& name, uint16_t type, uint16_t _class, uint32_t ttl, std::string const& server_name) :
      name(name), rr(type, _class, ttl, server_name) {}
  MdnsAnswer(std::string const& name, uint16_t type, uint16_t _class, uint32_t ttl, uint32_t server_address) :
      name(name), rr(type, _class, ttl, server_address) {}

  friend std::istream& operator>>(std::istream& is, MdnsAnswer& answer) {
    return is >> answer.name >> answer.rr;
  }

  friend std::ostream& operator<<(std::ostream& os, MdnsAnswer const& answer) {
    return os << answer.name << answer.rr;
  }

private:
  MdnsDomainName name;
  MdnsResourceRecord rr;    // TODO może dziedziczenie?
};  // class MdnsAnswer


class MdnsResponse {
public:
  /* konstruktor tworzy nagłówek mDNS i ustawia jego flagi na 0x8400 (bit QR i AA). */
  MdnsResponse() : header() {
    header.qr();
    header.aa();
  }

  /* dodanie odpowiedzi typu "PTR" */
  void add_answer(std::string const& query_name, QTYPE type, std::string const& server_name, uint16_t ttl) {
    header.ans_count(header.q_count() + 1);   // zwiększa licznik pytań w nagłówku  // TODO ładniej/efektywniej
    answers.push_back(MdnsAnswer(query_name, static_cast<uint16_t>(type), INTERNET_CLASS,
        ttl, server_name));
  }
  /* dodanie odpowiedzi typu "A" */
  void add_answer(std::string const& query_name, QTYPE type, uint16_t server_address, uint16_t ttl) {
    header.q_count(header.q_count() + 1);   // zwiększa licznik pytań w nagłówku  // TODO ładniej/efektywniej
    answers.push_back(MdnsAnswer(query_name, static_cast<uint16_t>(type), INTERNET_CLASS,
        ttl, server_address));
  }



  friend std::istream& operator>>(std::istream& is, MdnsResponse& response) {
    is >> response.header;
    for (int i = 0; i < response.header.ans_count(); i++) {
      MdnsAnswer answer;
      is >> answer;
      response.answers.push_back(std::move(answer));
    }
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, MdnsResponse const& response) {
    os << response.header;
    for (int i = 0; i < response.answers.size(); i++) {
      os << response.answers[i];
    }
    return os;
  }

private:
  MdnsHeader header;
  std::vector<MdnsAnswer> answers;
};  // class MdnsResponse


#endif  // MDNS_MESSAGE_H