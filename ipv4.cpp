#include <arpa/inet.h>
#include <cstdio>
#include <cstring>

#include "ipv4.hpp"

bool parse_ipv4_header(const ipv4_word_t * buffer, const size_t buffer_size, ipv4_header_s * output_header)
{
  bool          ret_val = true;
  unsigned int  i;
  uint_fast32_t computed_checksum = 0;
  ipv4_word_t   host_word;

  if(output_header)
  {
    memset(output_header, 0, sizeof(ipv4_header_s));
  }
  
  if(buffer && output_header && (BYTE_SIZE_TO_IPV4_WORD_SIZE(buffer_size) >= 5))
  {
    host_word = ntohl(buffer[0]);
    computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
    output_header->version         = (host_word>>28);
    output_header->ihl             = (host_word>>24);
    output_header->dscp            = (host_word>>18);
    output_header->ecn             = (host_word>>16);
    output_header->total_length    = (host_word);
    host_word = ntohl(buffer[1]);
    computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
    output_header->identification  = (host_word>>16);
    output_header->flags           = (host_word>>13);
    output_header->fragment_offset = (host_word);
    host_word = ntohl(buffer[2]);
    computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
    output_header->ttl             = (host_word>>24);
    output_header->protocol        = (host_word>>16);
    output_header->checksum        = (host_word);
    host_word = ntohl(buffer[3]);
    computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
    output_header->source_ip       = host_word;
    host_word = ntohl(buffer[4]);
    computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
    output_header->dest_ip         = host_word;

    for(i = 5; i < output_header->ihl; i++)
    {
      if(i < BYTE_SIZE_TO_IPV4_WORD_SIZE(buffer_size))
      {
        host_word = ntohl(buffer[i]);
        computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
        output_header->options[i] = host_word;
      }
      else
      {
        fprintf(stderr, "IPv4 buffer too small for header with options.  buffer 0x%x, buffer_size %u, ihl %u\n",
                 buffer, buffer_size, output_header->ihl);
        ret_val = false;
        break;
      }
    }

    if( (output_header->ihl < 5) || 
        (output_header->total_length < IPV4_WORD_SIZE_TO_BYTE_SIZE(output_header->ihl)))
    {
      fprintf(stderr, "Unexpected IPv4 header ihl (%u) or total_length (%u)\n",
              output_header->ihl, output_header->total_length);
      ret_val = false;
    }

    computed_checksum = (computed_checksum & 0xFFFF) + (computed_checksum>>16);

    if(0xFFFF != computed_checksum)
    {
      fprintf(stderr, "IPv4 checksum failed. computed_checksum 0x%x\n", computed_checksum);
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Bad IPv4 buffer inputs.  buffer 0x%x, buffer_size %u, output_header 0x%x\n",
            buffer, buffer_size, output_header);
    ret_val = false;
  }

  return ret_val;
}

ipv4_packet_meta_s parse_ipv4_packet(const ipv4_word_t * buffer, const size_t buffer_size)
{
  ipv4_packet_meta_s packet_meta = {0};

  if(buffer)
  {
    packet_meta.buffer      = buffer;
    packet_meta.buffer_size = buffer_size;

    packet_meta.header_valid = parse_ipv4_header(buffer, buffer_size, &packet_meta.header);

    if(packet_meta.header_valid && (packet_meta.header.ihl < BYTE_SIZE_TO_IPV4_WORD_SIZE(buffer_size)))
    {
      packet_meta.payload.buffer        = &buffer[packet_meta.header.ihl];
      packet_meta.payload.size          = (packet_meta.header.total_length-IPV4_WORD_SIZE_TO_BYTE_SIZE(packet_meta.header.ihl));
      packet_meta.payload.size_in_words = (BYTE_SIZE_TO_IPV4_WORD_SIZE(buffer_size)-packet_meta.header.ihl);
    }
  }

  return packet_meta;
}

ipv4_word_size_t get_ipv4_header_size(const ipv4_header_s* ipv4_header)
{
  ipv4_word_size_t ret_val = 0;

  if(ipv4_header)
  {
    return ipv4_header->ihl;
  }
  else
  {
    fprintf(stderr, "Invalid ipv4_header");
  }

  return ret_val;
}