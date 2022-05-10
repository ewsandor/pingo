#ifndef __ICMP_HPP__
#define __ICMP_HPP__

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

  /* Bad IP Header */
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
  
} icmp_rest_of_header_request_reply_s;

typedef union __attribute__((packed))
{
  icmp_rest_of_header_request_reply_s request_reply;

} icmp_rest_of_header_u;

typedef struct __attribute__((packed))
{
  uint8_t               type;
  icmp_code_e           code_enum:8;
  uint16_t              checksum;

  icmp_rest_of_header_u rest_of_header;
  
} icmp_header_s;


#endif /* __ICMP_HPP__ */