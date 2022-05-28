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
      icmp_packet_meta.payload      = (icmp_buffer_t*) &ipv4_payload->buffer[2];
      icmp_packet_meta.payload_size = (ipv4_payload->size-8);
    }
  }
  else
  {
    fprintf(stderr, "Null ipv4_payload passed to parse ICMP packet\n");
  }

  return icmp_packet_meta;
}

size_t encode_icmp_packet(const icmp_packet_meta_s* icmp_packet_meta, icmp_buffer_t * buffer, size_t buffer_size)
{
  size_t        output_size = 0;
  uint_fast32_t computed_checksum = 0;
  icmp_buffer_t *write_ptr;
  uint16_t      *checksum_ptr;
  size_t        tmp_size;
  uint16_t     *checksum_buffer;
  unsigned int  checksum_iterator;

  if(icmp_packet_meta && buffer)
  {
    output_size = (sizeof(icmp_header_s) + icmp_packet_meta->payload_size);

    if(icmp_packet_meta->header_valid && (output_size <= buffer_size))
    {
      memset(buffer, 0, output_size);
      write_ptr = buffer;
      *write_ptr = icmp_packet_meta->header.type;
      write_ptr++;
      *write_ptr = icmp_packet_meta->header.code;
      write_ptr++;
      /* Comeback to checksum later */
      checksum_ptr = (uint16_t*) write_ptr;
      write_ptr+=sizeof(uint16_t);
      switch(icmp_packet_meta->header.type)
      {
        case ICMP_TYPE_ECHO_REQUEST:
        case ICMP_TYPE_ECHO_REPLY:
        case ICMP_TYPE_TIMESTAMP_REQUEST:
        case ICMP_TYPE_TIMESTAMP_REPLY:
        case ICMP_TYPE_ADDRESS_MASK_REQUEST:
        case ICMP_TYPE_ADDRESS_MASK_REPLY:
        {
          *(uint16_t*)write_ptr = htons(icmp_packet_meta->header.rest_of_header.id_seq_num.identifier);
          write_ptr+=sizeof(uint16_t);
          *(uint16_t*)write_ptr = htons(icmp_packet_meta->header.rest_of_header.id_seq_num.sequence_number);
          write_ptr+=sizeof(uint16_t);
          break;
        }
        case ICMP_TYPE_REDIRECT_MESSAGE:
        {
          *(uint32_t*)write_ptr = htonl(icmp_packet_meta->header.rest_of_header.redirect);
          write_ptr+=sizeof(uint32_t);
          break;
        }
        case ICMP_TYPE_DESTINATION_UNREACHABLE:
        {
          *(uint16_t*)write_ptr = htons(icmp_packet_meta->header.rest_of_header.dest_unreachable.unused);
          write_ptr+=sizeof(uint16_t);
          *(uint16_t*)write_ptr = htons(icmp_packet_meta->header.rest_of_header.dest_unreachable.next_hop_mtu);
          write_ptr+=sizeof(uint16_t);
          break;
        }
        default:
        {
          *(uint32_t*)write_ptr = htonl(icmp_packet_meta->header.rest_of_header.unused);
          write_ptr+=sizeof(uint32_t);
          break;
        }
      }
      if(icmp_packet_meta->payload_size)
      {
        memcpy(write_ptr, icmp_packet_meta->payload, icmp_packet_meta->payload_size);
        write_ptr+=sizeof(icmp_packet_meta->payload_size);
      }

      /* Compute checksum */
      tmp_size = output_size;
      checksum_buffer=(uint16_t*) buffer;
      checksum_iterator=0;
      while(tmp_size >= sizeof(uint16_t))
      {
        computed_checksum += ntohs(checksum_buffer[checksum_iterator]);
        checksum_iterator++;
        tmp_size -= sizeof(uint16_t);
      }
      if(tmp_size == sizeof(uint8_t))
      {
        computed_checksum += (ntohs(checksum_buffer[checksum_iterator]) & 0xFF00);
        checksum_iterator++;
        tmp_size -= sizeof(uint8_t);
      }
      computed_checksum = ~((computed_checksum & 0xFFFF) + (computed_checksum>>16));
      *checksum_ptr = htons(computed_checksum);
    }
    else
    {
      fprintf(stderr, "ICMP header invalid or output buffer too small.  header_valid %u output_size %lu buffer_size %lu\n",
              icmp_packet_meta->header_valid, output_size, buffer_size);
      output_size = 0;
    }
  }
  else
  {
    fprintf(stderr, "Null inputs to encode ICMP packet.  icmp_packet_meta 0x%p buffer 0x%p\n",
            icmp_packet_meta, buffer);
    output_size = 0;
  }

  return output_size;
}