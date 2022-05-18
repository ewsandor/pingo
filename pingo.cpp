#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "icmp.hpp"
#include "ipv4.hpp"

int main(int argc, char *argv[])
{
  int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  struct sockaddr_in write_addr = {0};

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


  unsigned int packet_id = 73;
  const char * payload = "Ping test.";
  while(1)
  {
    icmp_packet_meta_s icmp_packet_meta;
    memset(&icmp_packet_meta, 0, sizeof(icmp_packet_meta_s));

    icmp_packet_meta.header.type = ICMP_TYPE_ECHO_REQUEST;
    icmp_packet_meta.header.code = ICMP_CODE_ZERO;
    icmp_packet_meta.header.rest_of_header.id_seq_num.identifier      = 0xEDED;
    icmp_packet_meta.header.rest_of_header.id_seq_num.sequence_number = packet_id;
    icmp_packet_meta.header_valid = true;
    icmp_packet_meta.payload = (icmp_buffer_t*) payload;
    icmp_packet_meta.payload_size = 10;
    ssize_t icmp_packet_size = encode_icmp_packet(&icmp_packet_meta, (icmp_buffer_t*) buffer, sizeof(buffer));
    printf("ICMP Packet %u size %lu\n", packet_id++, icmp_packet_size);

    write_addr = {0};
    write_addr.sin_family      = AF_INET;
    write_addr.sin_port        = htons(IPPROTO_ICMP);
    write_addr.sin_addr.s_addr = htonl((10<<24) | (73<<16) | (6<<8) | 0);
    if(icmp_packet_size != sendto(sockfd, buffer, icmp_packet_size, 0, (sockaddr*)&write_addr, sizeof(write_addr)))
    {
      fprintf(stderr, "Failed to send to raw socket.  errno %u\n", errno);
    }

    sleep(1);
  }

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
    }
  }
}
