#ifndef __IPV4_HPP__
#define __IPV4_HPP__

typedef uint32_t     ipv4_word_t;
typedef unsigned int ipv4_word_size_t;

typedef struct __attribute__((packed))
{
  uint8_t  version:4;
  uint8_t  ihl:4;
  uint8_t  dscp:6;
  uint8_t  ecn:2;
  uint16_t total_length;

  uint16_t identification;
  uint16_t flags:3;
  uint16_t fragment_offset:13;

  uint8_t  ttl;
  uint8_t  protocol;
  uint16_t checksum;

  uint32_t source_ip;

  uint32_t dest_ip;

  uint32_t  options[10];

} ipv4_header_s;

typedef struct 
{
  /* Raw buffer for IPv4 packet. Assumed to be in network big-endian format. */
  const ipv4_word_t *buffer;
  ipv4_word_size_t   buffer_size;
  
  /* Header is valid if the buffer has been parsed successfully and checksum was correct */
  bool               header_valid;
  ipv4_header_s      header;

  /* Pointer to payload after the IPv4 header from buffer. Assumed to be in network big-endian format. */
  const ipv4_word_t *payload;
  /* Size of the payload.  
      Based on buffer size (buffer_size-header.ihl), not the total length from the IPv4 header. */
  ipv4_word_size_t   payload_size;

} ipv4_packet_meta_s;

#define BYTE_SIZE_TO_IPV4_WORD_SIZE(byte_size) (byte_size/sizeof(ipv4_word_t))
#define IPV4_WORD_SIZE_TO_BYTE_SIZE(word_size) (word_size*sizeof(ipv4_word_t))

/* Parses ipv4 header from raw buffer */
bool parse_ipv4_header(const ipv4_word_t * buffer, const size_t, ipv4_header_s * output_header);

/* Returns the size of the ipv4 header in words.  May be used to find offset to payload.  
    Assumes header is valid. Returns 0 in case of error*/
ipv4_word_size_t get_ipv4_header_size(const ipv4_header_s*);

#endif /* __IPV4_HPP__ */