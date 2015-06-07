#ifndef MDNS_MESSAGE_H
#define MDNS_MESSAGE_H

enum class QTYPE : uint16_t {
  A = 1,
  PTR = 12
};

const uint16_t INTERNET_CLASS = 0x0001;

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

  friend std::istream& operator>>(std::istream& is, MdnsDomainName& domain_name) {
    unsigned char next_length;
    char buffer[256];    // maksymalna długość nazwy domeny to 255

    /* wczytujemy kolejne nazwy domen. */
    is >> next_length;
    while (next_length != 0) {
      is.read(buffer, next_length);
      domain_name.data.push_back(std::string(buffer, next_length));    // TODO czy działa
      is >> next_length;
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



class MdnsResourceRecord {
  //TODO
};

/* Klasa reprezentująca pojedynczą odpowiedź. */
class MdnsAnswer {
  MdnsAnswer(std::string name, uint16_t type, uint16_t _class, uint32_t ttl, uint16_t rr_len) :
      name(name), type(type), _class(_class), ttl(ttl), rr_len(rr_len) {}

  friend std::istream& operator>>(std::istream& is, MdnsAnswer& answer) {
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, MdnsAnswer const& answer) {
    return os;
  }

private:
  MdnsDomainName name;
  uint16_t type;
  uint16_t _class;
  uint32_t ttl;
  uint16_t rr_len;
  MdnsResourceRecord rr_data;    // TODO
};  // class MdnsAnswer


class MdnsResponse {
  // TODO tak jak MdnsQuery
};  // class MdnsResponse


#endif  // MDNS_MESSAGE_H