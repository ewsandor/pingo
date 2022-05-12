#ifndef __ICMP_HPP__
#define __ICMP_HPP__

#include "ipv4.hpp"

typedef enum
{
  ICMP_TYPE_ECHO_REPLY                = 0,
  /* 1-2 reserved */
  ICMP_TYPE_DESTINATION_UNREACHABLE   = 3,
  ICMP_TYPE_SOURCE_QUENCH             = 4,
  ICMP_TYPE_REDIRECT_MESSAGE          = 5,
  /* 6 deprecated */
  /* 7 reserved */
  ICMP_TYPE_ECHO_REQUEST              = 8,
  ICMP_TYPE_ROUTER_ADVERTISEMENT      = 9,
  ICMP_TYPE_ROUTER_SOLICITATION       = 10,
  ICMP_TYPE_TIME_EXCEEDED             = 11,
  ICMP_TYPE_PARAMETER_PROBLEM         = 12,
  ICMP_TYPE_TIMESTAMP_REQUEST         = 13,
  ICMP_TYPE_TIMESTAMP_REPLY           = 14,
  ICMP_TYPE_INFORMATION_REQUEST       = 15,
  ICMP_TYPE_INFORMATION_REPLY         = 16,
  ICMP_TYPE_ADDRESS_MASK_REQUEST      = 17,
  ICMP_TYPE_ADDRESS_MASK_REPLY        = 18,
  /* 19 reserved for security */
  /* 20-29 reserved for robustness experiment */
  ICMP_TYPE_TRACEROUTE                = 30,
  /* 31-39 deprecated */
  ICMP_TYPE_PHOTURIS_SECURITY_FAILURE = 40,
  /* 41 experimental */
  ICMP_TYPE_EXTENDED_ECHO_REQUEST     = 42,
  ICMP_TYPE_EXTENDED_ECHO_REPLY       = 43,
  /* 44-252 reserved */
  /* 253-254 experimental */
  /* 255 reserved */

} icmp_type_e;

typedef enum
{
  ICMP_CODE_ZERO                                      = 0,
  
  /* Destination Unreachable */
  ICMP_CODE_DEST_NETWORK_UNREACHABLE                  = 0,
  ICMP_CODE_DEST_HOST_UNREACHABLE                     = 1,
  ICMP_CODE_DEST_PROTOCOL_UNREACHABLE                 = 2,
  ICMP_CODE_DEST_PORT_UNREACHABLE                     = 3,
  ICMP_CODE_FRAGMENTATION_REQ_AND_DF_FLAG             = 4,
  ICMP_CODE_SOURCE_ROUTE_FAILED                       = 5,
  ICMP_CODE_DEST_NETWORK_UNKNOWN                      = 6,
  ICMP_CODE_DEST_HOST_UNKNOWN                         = 7,
  ICMP_CODE_SOURCE_HOST_ISOLATED                      = 8,
  ICMP_CODE_NETWORK_ADMIN_PROHIBITED                  = 9,
  ICMP_CODE_HOST_ADMIN_PROHIBITED                     = 10,
  ICMP_CODE_NETWORK_UNREACHABLE_FOR_TOS               = 11,
  ICMP_CODE_HOST_UNREACHABLE_FOR_TOS                  = 12,
  ICMP_CODE_COMMUNICATION_ADMIN_PROHIBITED            = 13,
  ICMP_CODE_HOST_PRECEDENCE_VIOLATION                 = 14,
  ICMP_CODE_PRECEDENCE_CUTOFF_IN_EFFECT               = 15,

  /* Redirect Message */
  ICMP_CODE_REDIRECT_DATAGRAM_FOR_THE_NETWORK         = 0,
  ICMP_CODE_REDIRECT_DATAGRAM_FOR_THE_HOST            = 1,
  ICMP_CODE_REDIRECT_DATAGRAM_FOR_THE_TOS_AND_NETWORK = 2,
  ICMP_CODE_REDIRECT_DATAGRAM_FOR_THE_TOS_AND_HOST    = 3,

  /* Time Exceeded */
  ICMP_CODE_TTL_EXPIRED_IN_TRANSIT                    = 0,
  ICMP_CODE_FRAGMENT_REASSEMBLY_TIME_EXCEEDED         = 1,

  /* Parameter Problem: Bad IP Header */
  ICMP_CODE_POINTER_INDICATES_THE_ERROR               = 0,
  ICMP_CODE_MISSING_A_REQUIRED_OPTION                 = 1,
  ICMP_CODE_BAD_LENGTH                                = 2,

  /* Extended Echo Reply */
  ICMP_CODE_NO_ERROR                                  = 0,
  ICMP_CODE_MALFORMED_QUERY                           = 1,
  ICMP_CODE_NO_SUCH_INTERFACE                         = 2,
  ICMP_CODE_NO_SUCH_TABLE_ENTRY                       = 3,
  ICMP_CODE_MULTIPLE_INTERFACES_SATISFY_QUERY         = 4,
  
} icmp_code_e;

typedef struct __attribute__((packed))
{
  uint16_t identifier;
  uint16_t sequence_number;
  
} icmp_rest_of_header_id_seq_num_s;

typedef uint32_t icmp_ip_address_t;

typedef struct __attribute__((packed))
{
  uint16_t unused;
  uint16_t next_hop_mtu;
  
} icmp_rest_of_header_dest_unreachable_s;

typedef union __attribute__((packed))
{
  uint32_t                               unused;
  icmp_rest_of_header_id_seq_num_s       id_seq_num;
  icmp_ip_address_t                      redirect;
  icmp_rest_of_header_dest_unreachable_s dest_unreachable;
} icmp_rest_of_header_u;

typedef struct __attribute__((packed))
{
  icmp_type_e           type:8;
  icmp_code_e           code:8;
  uint16_t              checksum;

  icmp_rest_of_header_u rest_of_header;
  
} icmp_header_s;

typedef uint8_t icmp_buffer_t;

typedef struct 
{

  /* Header is valid if the ipv4 packet has been parsed successfully and checksum was correct */
  bool                 header_valid;
  icmp_header_s        header;

  const icmp_buffer_t *payload;
  size_t               payload_size;

} icmp_packet_meta_s;

icmp_packet_meta_s parse_icmp_packet(const ipv4_payload_s*);

size_t encoded_icmp_packet(const icmp_packet_meta_s*, icmp_buffer_t *, size_t);

#endif /* __ICMP_HPP__ */