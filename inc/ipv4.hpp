#ifndef __IPV4_HPP__
#define __IPV4_HPP__

#include <cstddef>
#include <cstdint>

#define IPV4_WORD_BITS_24         24
#define IPV4_HALF_WORD_BITS       16
#define IPV4_HALF_WORD_MASK   0xFFFF
#define IPV4_HALF_WORD_MASK_H 0xFF00
#define IPV4_HALF_WORD_MASK_L 0x00FF
typedef uint32_t     ipv4_word_t;
typedef unsigned int ipv4_word_size_t;
#define IPV4_HALF_WORD_SIZE_BYTES (sizeof(ipv4_word_t)/2)

#define IPV4_MAX_PACKET_SIZE_BYTES 65535
#define IPV4_MAX_PACKET_SIZE_WORDS ((IPV4_MAX_PACKET_SIZE_BYTES/sizeof(ipv4_word_size_t))+1)

#define IPV4_FLAG_DF 0x2
#define IPV4_FLAG_MF 0x4

#define IPV4_VERSION 4
#define IPV4_HEADER_FIXED_SIZE_WORDS 5
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
  /* Pointer to payload buffer. Assumed to be in network big-endian format. */
  const ipv4_word_t *buffer;
  /* Size of payload. 
      Based on total length (total_length - header_size), payload may be truncated if payload buffer is smaller than total length. */
  size_t             size;
  /* Size of the payload buffer.  
      Based on buffer size (buffer_size-header.ihl), not the total length from the IPv4 header. */
  ipv4_word_size_t   size_in_words;

} ipv4_payload_s;


typedef struct 
{
  /* Raw buffer for IPv4 packet. Assumed to be in network big-endian format. */
  const ipv4_word_t *buffer;
  /* Size of raw buffer in bytes */
  size_t             buffer_size;
  
  /* Header is valid if the buffer has been parsed successfully and checksum was correct */
  bool               header_valid;
  ipv4_header_s      header;

  ipv4_payload_s     payload;

} ipv4_packet_meta_s;

#define BYTE_SIZE_TO_IPV4_WORD_SIZE(byte_size) (byte_size/sizeof(ipv4_word_t))
#define IPV4_WORD_SIZE_TO_BYTE_SIZE(word_size) (word_size*sizeof(ipv4_word_t))

/* Parses ipv4 header from raw buffer */
bool parse_ipv4_header(const ipv4_word_t * buffer, const size_t, ipv4_header_s * output_header);

/* Parses IPv4 packet.  Returns metadata structure for packet. */
ipv4_packet_meta_s parse_ipv4_packet(const ipv4_word_t * buffer, const size_t);

/* Returns the size of the ipv4 header in words.  May be used to find offset to payload.  
    Assumes header is valid. Returns 0 in case of error*/
ipv4_word_size_t get_ipv4_header_size(const ipv4_header_s*);

size_t encode_ipv4_packet(const ipv4_packet_meta_s*, ipv4_word_t *, size_t);

#endif /* __IPV4_HPP__ */