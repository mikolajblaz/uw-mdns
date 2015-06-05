#ifndef MDNS_MESSAGE_H
#define MDNS_MESSAGE_H

enum class QTYPE {
  A = 1;
  PTR = 12;
};


std::vector<unsigned char*> read_domain_name(std::istream& is) {
  unsigned char next_length[1];
  unsigned char buffer[256];
  std::vector<std::string> result;

  is.read(next_length, 1);
  while (next_length[0] != 0) {
    is.read(buffer, next_length[0]);
    buffer[next_length[0]] = 0;
    result.push_back(std::string(buffer));    // TODO czy działa
    is.read(next_length, 1);
  }
  return result;
}

void write_domain_name(std::ostream& os, std::vector<std::string>& name) {
  for (int i = 0; i < name.size(); i++) {
    os << static_cast<unsigned char>(name[i].size());
    os << name[i];
  }
}


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
  qr() { data[2] = data[2] | 0x80; }     // query/response flag
  // pole OPCODE musi zawierać 0
  aa() { data[3] = data[3] | 0x40; }     // authoritive answer  
  tc() { data[3] = data[3] | 0x20; }     // truncated message
  // pole RD, RA, Z, AD, CD, RCODE musi zawierać 0

  unsigned uint16_t id()          { return data[0] << 8 + data[1]; };
  unsigned uint16_t q_count()     { return data[4] << 8 + data[5]; };
  unsigned uint16_t ans_count()   { return data[6] << 8 + data[7]; };
  unsigned uint16_t auth_count()  { return data[8] << 8 + data[9]; };
  unsigned uint16_t add_count()   { return data[10] << 8 + data[11]; };

  void id(unsigned uint16_t val)          { data[0] = val & 0x00FF; data[1] = val >> 8; };
  void q_count(unsigned uint16_t val)     { data[4] = val & 0x00FF; data[5] = val >> 8; };
  void ans_count(unsigned uint16_t val)   { data[6] = val & 0x00FF; data[7] = val >> 8; };
  void auth_count(unsigned uint16_t val)  { data[8] = val & 0x00FF; data[9] = val >> 8; };
  void add_count(unsigned uint16_t val)   { data[10] = val & 0x00FF; data[11] = val >> 8; };

  friend std::istream& operator>>(std::istream& is, MdnsHeader& header) {
    return is.read(reinterpret_cast<char*>(header.data), header_length);
  }

  friend std::ostream& operator<<(std::ostream& os, const MdnsHeader& header) {
    return os.write(reinterpret_cast<const char*>(header.data), header_length);
  }

private:
  static const streamsize header_length = 12;    // dł. nagłówka w bajtach;
  unsigned char data[header_length];             // dane nagłówka
};


class ResourceRecord {
  ResourceRecord(std::string name, unsigned uint16_t type, unsigned uint16_t _class,
      unsigned uint32_t ttl, unsigned uint16_t flags_len) :
      name(name.str()), type(type), _class(_class), ttl(ttl), flags_len(flags_len) {}

  ResourceRecord(std::istream& is) {
    is >> this;
  }

  friend std::istream& operator>>(std::istream& is, MdnsHeader& header) {
    name = read_domain_name(is);
    return is.read(reinterpret_cast<char*>(header.data), header_length);
  }

  friend std::ostream& operator<<(std::ostream& os, const MdnsHeader& header) {
    return os.write(reinterpret_cast<const char*>(header.data), header_length);
  }

private:
  static streamsize rr_length = 10;
  std::vector<std::string> name;
  unsigned char data[rr_length];
};


class MdnsQuestion {
public:
  MdnsQuestion() {}

private:
  unsigned uint16_t qtype;
  unsigned uint16_t qclass;
};



//Constant sized fields of query structure
struct QUESTION
{
  unsigned uint16_t qtype;
  unsigned uint16_t qclass;
};

//Constant sized fields of the resource record structure
#pragma pack(push, 1)
struct R_flags
{
  unsigned uint16_t type;
  unsigned uint16_t _class;
  unsigned uint32_t ttl;
  unsigned uint16_t flags_len;
};
#pragma pack(pop)

//Pointers to resource record contents
struct RES_RECORD
{
  unsigned char *name;
  struct R_flags *resource;
  unsigned char *rflags;
};

//Structure of a Query
typedef struct
{
  unsigned char *name;
  struct QUESTION *ques;
} QUERY;

#endif  // MDNS_MESSAGE_H