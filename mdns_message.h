#ifndef MDNS_MESSAGE_H
#define MDNS_MESSAGE_H

class MdnsHeader {

public:
  enum { echo_reply = 0, destination_unreachable = 3, source_quench = 4,
    redirect = 5, echo_request = 8, time_exceeded = 11, parameter_problem = 12,
    timestamp_request = 13, timestamp_reply = 14, info_request = 15,
    info_reply = 16, address_request = 17, address_reply = 18 };

  MdnsHeader() {
    std::fill(flags, flags + sizeof(flags), 0);
  }

  /* gettery flag: */
  bool qr() const { return get_flags_bit(0); }     // query/response flag
  bool opcode() const { return get_flags_bit(4); } // purpose of message     // TODO czy niepuste
  bool aa() const { return get_flags_bit(5); }     // authoritive answer  
  bool tc() const { return get_flags_bit(6); }     // truncated message
  // pole RD, RA, Z, AD, CD jest ignorowane
  bool rcode() const { return get_flags_bit(15); } // response code           // TODO

  /* settery: */
  qr(bool val) { set_flags_bit(0, val) }     // query/response flag
  // pole OPCODE musi zawierać 0
  aa(bool val) { set_flags_bit(5, val) }     // authoritive answer  
  tc(bool val) { set_flags_bit(6, val) }     // truncated message
  // pole RD, RA, Z, AD, CD, RCODE musi zawierać 0


  friend std::istream& operator>>(std::istream& is, MdnsHeader& header) {
    return is.read(reinterpret_cast<char*>(header.flags), 8);
  }

  friend std::ostream& operator<<(std::ostream& os, const MdnsHeader& header) {
    return os.write(reinterpret_cast<const char*>(header.flags), 8);
  }

private:
  
  
  static const streamsize length = 12;    // dł. nagłówka w bajtach;
  unsigned char data[length];             // dane

  // unsigned uint16_t id;         // identification number
  // unsigned uint16_t flags;      // flags
  // unsigned uint16_t q_count;    // number of question entries
  // unsigned uint16_t ans_count;  // number of answer entries
  // unsigned uint16_t auth_count; // number of authority entries
  // unsigned uint16_t add_count;  // number of resource entries

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