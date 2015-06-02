#ifndef MDNS_HEADER_H
#define MDNS_HEADER_H

class MdnsHeader {

public:
  enum { echo_reply = 0, destination_unreachable = 3, source_quench = 4,
    redirect = 5, echo_request = 8, time_exceeded = 11, parameter_problem = 12,
    timestamp_request = 13, timestamp_reply = 14, info_request = 15,
    info_reply = 16, address_request = 17, address_reply = 18 };

  MdnsHeader() {
    std::fill(rep_, rep_ + sizeof(rep_), 0);
  }


  unsigned short id; // identification number

  unsigned char rd() const { return data; }; // recursion desired
  unsigned char tc() const { return data; }; // truncated message
  unsigned char aa() const { return data; }; // authoritive answer
  unsigned char opcode() const { return data; }; // purpose of message
  unsigned char qr() const { return data; }; // query/response flag

  unsigned char rcode() const { return data; }; // response code
  unsigned char cd() const { return data; }; // checking disabled
  unsigned char ad() const { return data; }; // authenticated data
  unsigned char z() const { return data; }; // its z! reserved
  unsigned char ra() const { return data; }; // recursion available

  unsigned short q_count; // number of question entries
  unsigned short ans_count; // number of answer entries
  unsigned short auth_count; // number of authority entries
  unsigned short add_count; // number of resource entries


  unsigned char type() const { return rep_[0]; }
  unsigned char code() const { return rep_[1]; }
  unsigned short checksum() const { return decode(2, 3); }
  unsigned short identifier() const { return decode(4, 5); }
  unsigned short sequence_number() const { return decode(6, 7); }

  void type(unsigned char n) { rep_[0] = n; }
  void code(unsigned char n) { rep_[1] = n; }
  void checksum(unsigned short n) { encode(2, 3, n); }
  void identifier(unsigned short n) { encode(4, 5, n); }
  void sequence_number(unsigned short n) { encode(6, 7, n); }

  friend std::istream& operator>>(std::istream& is, icmp_header& header) {
    return is.read(reinterpret_cast<char*>(header.rep_), 8);
  }

  friend std::ostream& operator<<(std::ostream& os, const icmp_header& header) {
    return os.write(reinterpret_cast<const char*>(header.rep_), 8);
  }

private:
  unsigned short decode(int a, int b) const {
    return (rep_[a] << 8) + rep_[b];
  }

  void encode(int a, int b, unsigned short n) {
    rep_[a] = static_cast<unsigned char>(n >> 8);
    rep_[b] = static_cast<unsigned char>(n & 0xFF);
  }

  unsigned char rep_[8];

};

#endif  // MDNS_HEADER_H



//DNS header structure
struct DNS_HEADER
{
  unsigned short id; // identification number

  unsigned char rd :1; // recursion desired
  unsigned char tc :1; // truncated message
  unsigned char aa :1; // authoritive answer
  unsigned char opcode :4; // purpose of message
  unsigned char qr :1; // query/response flag

  unsigned char rcode :4; // response code
  unsigned char cd :1; // checking disabled
  unsigned char ad :1; // authenticated data
  unsigned char z :1; // its z! reserved
  unsigned char ra :1; // recursion available

  unsigned short q_count; // number of question entries
  unsigned short ans_count; // number of answer entries
  unsigned short auth_count; // number of authority entries
  unsigned short add_count; // number of resource entries
};

//Constant sized fields of query structure
struct QUESTION
{
  unsigned short qtype;
  unsigned short qclass;
};

//Constant sized fields of the resource record structure
#pragma pack(push, 1)
struct R_DATA
{
  unsigned short type;
  unsigned short _class;
  unsigned int ttl;
  unsigned short data_len;
};
#pragma pack(pop)

//Pointers to resource record contents
struct RES_RECORD
{
  unsigned char *name;
  struct R_DATA *resource;
  unsigned char *rdata;
};

//Structure of a Query
typedef struct
{
  unsigned char *name;
  struct QUESTION *ques;
} QUERY;
