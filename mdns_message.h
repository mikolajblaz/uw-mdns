#ifndef MDNS_MESSAGE_H
#define MDNS_MESSAGE_H

struct InvalidMdnsMessageException : public std::runtime_error {
  InvalidMdnsMessageException() : std::runtime_error("Invalid mDNS message format") {};
  InvalidMdnsMessageException(std::string const& what) : std::runtime_error(what) {};
};

enum class QTYPE : uint16_t {
  A = 1,
  PTR = 12
};

const uint16_t INTERNET_CLASS = 0x0001;
const int MAX_DOMAINS_DEPTH = 10;    // maksymalna dpouszczalna głębokość drzewa domenowego
const int MAX_DOMAIN_LENGTH = 255;   // maksymalna długość nazwy domeny w bajtach

/* Wypisuje 'val' na strumień 'os' w formacie big endian. */
template <typename uintX_t>
inline std::ostream& write_be(std::ostream& os, uintX_t val) {
  unsigned char c[sizeof(uintX_t)];
  /* wpisujemy kolejne bajty do tablicy, począwszy od najstarszego: */
  for (int i = 0; i < sizeof(uintX_t); i++) {
    c[i] = static_cast<unsigned char>(val >> CHAR_BIT * (sizeof(uintX_t) - 1 - i));
  }
  /* wypisujemy całość: */
  os.write(reinterpret_cast<const char *>(c), sizeof(uintX_t));
  return os;
}

/* Czyta 'val' ze strumienia 'is' zapisane w formacie big endian. */
template <typename uintX_t>
inline std::istream& read_be(std::istream& is, uintX_t& val) {
  unsigned char c[sizeof(uintX_t)];
  val = 0;
  /* wczytujemy wszystkie bajty: */
  is.read(reinterpret_cast<char *>(c), sizeof(uintX_t));
  /* interpretujemy je jako zapisane w kolejności big-endian: */
  for (int i = 0; i < sizeof(uintX_t); i++) {
    val += c[i] << CHAR_BIT * (sizeof(uintX_t) - 1 - i);
  }
  return is;
}


/* Klasa reprezentująca nagłówek mDNS. */
class MdnsHeader {
public:
  MdnsHeader() {
    std::fill(data, data + sizeof(data), 0);
  }

  /* Sprawdza poprawność nagłowka zapytania mDNS: */
  bool valid_query_header()   { return !qr() && !opcode() && !rcode(); }
  /* Sprawdza poprawność nagłowka odpowiedzi mDNS: */
  bool valid_response_header() { return qr() && !opcode() && !rcode(); }

  /* gettery flag (w OPCODE i RCODE sprawdzamy tylko niepustość): */
  bool qr() const { return data[2] & 0x80; }     // query/response flag
  bool opcode() const { return data[2] & 0x78; } // purpose of message
  bool aa() const { return data[2] & 0x04; }     // authoritive answer  
  bool tc() const { return data[2] & 0x02; }     // truncated message
  // pole RD, RA, Z, AD, CD jest ignorowane
  bool rcode() const { return data[3] & 0x0F; }  // response code

  // /* settery: */
  void set_qr() { data[2] = data[2] | 0x80; }     // query/response flag
  // pole OPCODE musi zawierać 0
  void set_aa() { data[2] = data[2] | 0x04; }     // authoritive answer  
  void set_tc() { data[2] = data[2] | 0x02; }     // truncated message
  // pole RD, RA, Z, AD, CD, RCODE musi zawierać 0

  uint16_t id()          { return (data[0] << CHAR_BIT) + data[1]; }
  uint16_t q_count()     { return (data[4] << CHAR_BIT) + data[5]; }
  uint16_t ans_count()   { return (data[6] << CHAR_BIT) + data[7]; }
  uint16_t auth_count()  { return (data[8] << CHAR_BIT) + data[9]; }
  uint16_t add_count()   { return (data[10] << CHAR_BIT) + data[11]; }

  void id(uint16_t val)          { data[0] = val >> CHAR_BIT; data[1] = val & 0x00FF; }
  void q_count(uint16_t val)     { data[4] = val >> CHAR_BIT; data[5] = val & 0x00FF; }
  void ans_count(uint16_t val)   { data[6] = val >> CHAR_BIT; data[7] = val & 0x00FF; }
  void auth_count(uint16_t val)  { data[8] = val >> CHAR_BIT; data[9] = val & 0x00FF; }
  void add_count(uint16_t val)   { data[10] = val >> CHAR_BIT; data[11] = val & 0x00FF; }

  friend std::istream& operator>>(std::istream& is, MdnsHeader& header) {
    return is.read(reinterpret_cast<char*>(header.data), MdnsHeader::header_length);
  }

  friend std::ostream& operator<<(std::ostream& os, MdnsHeader const& header) {
    return os.write(reinterpret_cast<const char*>(header.data), MdnsHeader::header_length);
  }

private:
  static const std::streamsize header_length = 12;    // dł. nagłówka w bajtach;
  unsigned char data[header_length];                  // dane nagłówka
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

  uint16_t size() const {
    uint16_t result = 1;              // ostatni zerowy bajt
    for (int i = 0; i < data.size(); i++) {
      result += data[i].size() + 1;   // + 1 to bajt zliczający długość
    }
    return result;
  }

  friend std::istream& operator>>(std::istream& is, MdnsDomainName& domain_name) {
    unsigned char next_length;
    char buffer[MAX_DOMAIN_LENGTH];

    /* wczytujemy kolejne nazwy domen. */
    read_be(is, next_length);
    while (next_length != 0 && domain_name.data.size() <= MAX_DOMAINS_DEPTH) {
      is.read(buffer, next_length);
      domain_name.data.push_back(std::string(buffer, next_length));    // TODO czy działa
      read_be(is, next_length);
    }
    if (domain_name.data.size() > MAX_DOMAINS_DEPTH)
      throw InvalidMdnsMessageException("Too long fully qualified domain name");
    
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

  friend bool operator==(MdnsDomainName const& name1, MdnsDomainName const& name2) {
    if (name1.data.size() != name2.data.size()) {
      return false;
    } else {
      int i = 0;
      while (i < name1.data.size() && name1.data[i] == name2.data[i])
        i++;
      return i == name1.data.size();
    }
  }

  /* Porównanie według kolejnych nazw domenowych. */
  friend bool operator<(MdnsDomainName const& name1, MdnsDomainName const& name2) {
    if (name1.data.size() == name2.data.size()) {
      int i = 0;
      while (i < name1.data.size() && name1.data[i] == name2.data[i])
        i++;
      if (i == name1.data.size())   // name1 == name2
        return false;
      else
        return name1.data[i] < name2.data[i];
    } else {
      return name1.data.size() < name2.data.size();
    }
  }

private:
  std::vector<std::string> data;
};  // class MdnsDomainName


/* Klasa reprezentująca pojedyncze zapytanie mDNS. */
class MdnsQuestion {
public:
  MdnsQuestion() {}
  MdnsQuestion(MdnsDomainName const& domain_name, uint16_t qtype, uint16_t qclass = INTERNET_CLASS) :
      name(domain_name), qtype(qtype), qclass(qclass) {}

  MdnsDomainName get_name() const { return name; }
  uint16_t get_qtype() const { return qtype; }


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

  const std::vector<MdnsQuestion>& get_questions() const { return questions; }

  void add_question(MdnsDomainName const& domain_name, QTYPE type) {
    header.q_count(header.q_count() + 1);   // zwiększa licznik pytań w nagłówku  // TODO ładniej/efektywniej
    questions.push_back(MdnsQuestion(domain_name, static_cast<uint16_t>(type)));
  }

  bool try_read(std::istream& is) {
    is >> header;
    if (header.qr()) {            // to nie jest zapytanie
      return false;
    } else {
      read_questions(is);   // zapytanie
      return true;
    }
  }

  friend std::istream& operator>>(std::istream& is, MdnsQuery& query) {
    is >> query.header;
    query.read_questions(is);
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
  void read_questions(std::istream& is) {
    if (!header.valid_query_header())
      throw InvalidMdnsMessageException("Invalid mDNS query header");
    for (int i = 0; i < header.q_count(); i++) {
      MdnsQuestion question;
      is >> question;
      questions.push_back(std::move(question));
    }
  }

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
  MdnsResourceRecord(uint16_t type, uint16_t _class, uint32_t ttl, MdnsDomainName const& server_name) :
      type(type), _class(_class), ttl(ttl), rr_len(server_name.size()), server_name(server_name) {
        if (type != static_cast<uint16_t>(QTYPE::PTR))
          throw InvalidMdnsMessageException("Improper MdnsResourceRecord constructor used for RR type PTR");
      }
  /* rekord typu "A" - nieużywana nazwa */
  MdnsResourceRecord(uint16_t type, uint16_t _class, uint32_t ttl, uint32_t server_address) :
      type(type), _class(_class), ttl(ttl), rr_len(sizeof(server_address)), server_address(server_address) {
        if (type != static_cast<uint16_t>(QTYPE::A))
          throw InvalidMdnsMessageException("Improper MdnsResourceRecord constructor used for RR type A");
      }

  uint16_t get_type() const { return type; }
  uint16_t get__class() const { return _class; }
  uint32_t get_ttl() const { return ttl; }
  uint16_t get_rr_len() const { return rr_len; }
  MdnsDomainName get_server_name() const { return server_name; }  // dla typu PTR
  uint32_t get_server_address() const { return server_address; }  // dla typu A

  friend std::istream& operator>>(std::istream& is, MdnsResourceRecord& rr) {
    read_be(is, rr.type);
    read_be(is, rr._class);
    read_be(is, rr.ttl);
    read_be(is, rr.rr_len);
    switch (rr.type) {
      case static_cast<uint16_t>(QTYPE::PTR): is >> rr.server_name; break;
      case static_cast<uint16_t>(QTYPE::A): read_be(is, rr.server_address); break;
      default: throw InvalidMdnsMessageException("Unrecognized RR type");
    }
    if (rr.server_name.size() != rr.rr_len)
      throw InvalidMdnsMessageException("Invalid server name length");
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, MdnsResourceRecord const& rr) {
    write_be(os, rr.type);
    write_be(os, rr._class);
    write_be(os, rr.ttl);
    write_be(os, rr.rr_len);
    switch (rr.type) {
      case static_cast<uint16_t>(QTYPE::PTR): os << rr.server_name; break;
      case static_cast<uint16_t>(QTYPE::A): write_be(os, rr.server_address); break;
      default: throw InvalidMdnsMessageException("Unrecognized RR type");
    }
    return os;
  }

private:
  uint16_t type;                // typ rekordu
  uint16_t _class;
  uint32_t ttl;
  uint16_t rr_len;
  /* W zależności od rodzaju zapytania używana jest jedna z dwóch zmiennych: */
  MdnsDomainName server_name;   // dla rekordu typu "PTR"
  uint32_t server_address;      // dla rekordu typu "A"
};

/* Klasa reprezentująca pojedynczą odpowiedź. */
class MdnsAnswer {
public:
  MdnsAnswer() {}
  MdnsAnswer(MdnsDomainName const& name, uint16_t type, uint16_t _class, uint32_t ttl, MdnsDomainName const& server_name) :
      name(name), rr(type, _class, ttl, server_name) {}
  MdnsAnswer(MdnsDomainName const& name, uint16_t type, uint16_t _class, uint32_t ttl, uint32_t server_address) :
      name(name), rr(type, _class, ttl, server_address) {}

  MdnsDomainName get_name() const { return name; }
  uint16_t get_type() const { return rr.get_type(); }
  uint16_t get_class() const { return rr.get__class(); }
  uint32_t get_ttl() const { return rr.get_ttl(); }
  uint16_t get_rr_len() const { return rr.get_rr_len(); }
  MdnsDomainName get_server_name() const { return rr.get_server_name(); }  // dla typu PTR
  uint32_t get_server_address() const { return rr.get_server_address(); }  // dla typu A

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
    header.set_qr();
    header.set_aa();
  }

  const std::vector<MdnsAnswer>& get_answers() const { return answers; }

  /* dodanie gotowej odpowiedzi klasy Answer. */
  void add_answer(MdnsAnswer const& answer) {
    header.ans_count(header.q_count() + 1);   // zwiększa licznik pytań w nagłówku  // TODO ładniej/efektywniej
    answers.push_back(answer);
  }

  /* dodanie gotowej odpowiedzi klasy Answer. */
  void add_answer(MdnsAnswer&& answer) {
    header.ans_count(header.q_count() + 1);   // zwiększa licznik pytań w nagłówku  // TODO ładniej/efektywniej
    answers.push_back(answer);
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

  bool try_read(std::istream& is) {
    is >> header;
    if (!header.qr()) {           // to nie jest odpowiedź
      return false;
    } else {
      read_answers(is);         // odpowiedź
      return true;
    }
  }

  friend std::istream& operator>>(std::istream& is, MdnsResponse& response) {
    is >> response.header;
    response.read_answers(is);
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
  void read_answers(std::istream& is) {
    if (!header.valid_response_header())
      throw InvalidMdnsMessageException("Invalid mDNS response header");
    for (int i = 0; i < header.ans_count(); i++) {
      MdnsAnswer answer;
      is >> answer;
      answers.push_back(std::move(answer));
    }
  }

  MdnsHeader header;
  std::vector<MdnsAnswer> answers;
};  // class MdnsResponse


#endif  // MDNS_MESSAGE_H