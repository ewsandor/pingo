#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>

#include "icmp.hpp"
#include "ipv4.hpp"

int main(int argc, char *argv[])
{
  int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if(sockfd == -1)
  {
    switch(errno)
    {
      case EPERM:
      {
        fprintf(stderr, "No permission to open raw socket.\n");
        break;
      }
      default:
      {
        fprintf(stderr, "Failed to open raw socket.  errno %u\n", errno);
        break;
      }
    }
    exit(1);
  }

  ipv4_word_t buffer[IPV4_MAX_PACKET_SIZE_WORDS];

  while(1)
  {
    memset(&buffer, 0, sizeof(buffer));

    ipv4_packet_meta_s ipv4_packet_meta;
    icmp_packet_meta_s icmp_packet_meta;

    recv(sockfd, &buffer, sizeof(buffer), 0);

    ipv4_packet_meta = parse_ipv4_packet(buffer, sizeof(buffer));

    printf("IPv4 valid %u src 0x%x dest 0x%x id 0x%x total_length %u ttl %u\n", 
            ipv4_packet_meta.header_valid,
            ipv4_packet_meta.header.source_ip, 
            ipv4_packet_meta.header.dest_ip, 
            ipv4_packet_meta.header.identification,
            ipv4_packet_meta.header.total_length,
            ipv4_packet_meta.header.ttl);

    if(ipv4_packet_meta.header_valid)
    {
      icmp_packet_meta = parse_icmp_packet(&ipv4_packet_meta.payload);
      printf("ICMP valid %u type %u code %u id %u seq_num %u payload_size %lu\n", 
              icmp_packet_meta.header_valid,
              icmp_packet_meta.header.type, 
              icmp_packet_meta.header.code, 
              icmp_packet_meta.header.rest_of_header.id_seq_num.identifier,
              icmp_packet_meta.header.rest_of_header.id_seq_num.sequence_number,
              icmp_packet_meta.payload_size);

      encode_icmp_packet(&icmp_packet_meta, (icmp_buffer_t*) buffer, sizeof(buffer));
      encode_ipv4_packet(&ipv4_packet_meta, buffer, sizeof(buffer));
    }
  }
}
