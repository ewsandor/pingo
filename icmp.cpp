#include <arpa/inet.h>
#include <cstdio>
#include <cstring>

#include "icmp.hpp"

inline bool parse_icmp_header(const ipv4_payload_s* ipv4_payload, icmp_header_s * output_header)
{
  bool             ret_val = true;
  uint_fast32_t    computed_checksum = 0;
  ipv4_word_t      host_word;
  size_t           tmp_size;
  ipv4_word_size_t tmp_size_in_words;

  if(ipv4_payload && output_header)
  {
    if((ipv4_payload->size >= 8) && 
       (ipv4_payload->size_in_words >= 2))
    {
      host_word = ntohl(ipv4_payload->buffer[0]);
      computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
      output_header->type     = (icmp_type_e)(host_word >> 24);
      output_header->code     = (icmp_code_e)(host_word >> 16);
      output_header->checksum = host_word;
      host_word = ntohl(ipv4_payload->buffer[1]);
      computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
      switch(output_header->type)
      {
        case ICMP_TYPE_ECHO_REQUEST:
        case ICMP_TYPE_ECHO_REPLY:
        case ICMP_TYPE_TIMESTAMP_REQUEST:
        case ICMP_TYPE_TIMESTAMP_REPLY:
        case ICMP_TYPE_ADDRESS_MASK_REQUEST:
        case ICMP_TYPE_ADDRESS_MASK_REPLY:
        {
          output_header->rest_of_header.id_seq_num.identifier      = (host_word >> 16);
          output_header->rest_of_header.id_seq_num.sequence_number = host_word;
          break;
        }
        case ICMP_TYPE_REDIRECT_MESSAGE:
        {
          output_header->rest_of_header.redirect = host_word;
          break;
        }
        case ICMP_TYPE_DESTINATION_UNREACHABLE:
        {
          output_header->rest_of_header.dest_unreachable.unused       = (host_word >> 16);
          output_header->rest_of_header.dest_unreachable.next_hop_mtu = host_word;
          break;
        }
         default:
        {
          output_header->rest_of_header.unused = host_word;
          break;
        }
      }

      tmp_size = (ipv4_payload->size-8);
      tmp_size_in_words = 2;
      while((tmp_size >= sizeof(ipv4_word_t)) &&
            (tmp_size_in_words < ipv4_payload->size_in_words))
      {
        host_word = ntohl(ipv4_payload->buffer[tmp_size_in_words]);
        computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
        tmp_size -= sizeof(ipv4_word_t);
        tmp_size_in_words++;
      }
      if(tmp_size && (tmp_size_in_words < ipv4_payload->size_in_words))
      {
        host_word = ntohl(ipv4_payload->buffer[tmp_size_in_words]);
        if(tmp_size >= 2)
        {
          computed_checksum += (host_word>>16);
          tmp_size -= 2;
          if(tmp_size == 1)
          {
            computed_checksum += (host_word & 0xFF00);
            tmp_size -= 1;
          }
        }
        else if(tmp_size == 1)
        {
          computed_checksum += ((host_word>>16) & 0xFF00);
          tmp_size -= 1;
        }
      }
      if(tmp_size != 0)
      {
        fprintf(stderr, "Failed to compute checksum for payload. tmp_size %lu payload_size %lu buffer_size_in_words %u\n", 
                tmp_size, ipv4_payload->size, ipv4_payload->size_in_words);
        ret_val = false; 
      }
      computed_checksum = (computed_checksum & 0xFFFF) + (computed_checksum>>16);
      if(0xFFFF != computed_checksum)
      {
        fprintf(stderr, "ICMP checksum failed. computed_checksum 0x%lx\n", computed_checksum);
        ret_val = false;
      }
    }
    else
    {
      fprintf(stderr, "IPv4 payload too small for ICMP header. size %lu size_in_words %u\n",
              ipv4_payload->size, ipv4_payload->size_in_words);
      ret_val = false; 
    }
  }
  else
  {
    fprintf(stderr, "Nullptr passed to parse ICMP header.  ipv4_payload 0x%p output_header 0x%p\n",
            ipv4_payload, output_header);
    ret_val = false;
  }

  return ret_val;
}

icmp_packet_meta_s parse_icmp_packet(const ipv4_payload_s* ipv4_payload)
{
  icmp_packet_meta_s icmp_packet_meta;
  memset(&icmp_packet_meta, 0, sizeof(icmp_packet_meta_s));

  if(ipv4_payload)
  {
    icmp_packet_meta.header_valid = parse_icmp_header(ipv4_payload, &icmp_packet_meta.header);
    if(icmp_packet_meta.header_valid)
    {
      icmp_packet_meta.payload      = (icmp_payload_t*) &ipv4_payload->buffer[2];
      icmp_packet_meta.payload_size = (ipv4_payload->size-8);
    }
  }
  else
  {
    fprintf(stderr, "Null ipv4_payload passed to parse ICMP packet\n");
  }

  return icmp_packet_meta;
}