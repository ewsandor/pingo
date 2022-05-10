#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

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

int main(int argc, char *argv[])
{
  int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if(sockfd == -1)
  {
    switch(errno)
    {
      case EPERM:
      {
        fprintf(stderr, "No permission to open raw socket.\n", errno);
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

  uint32_t buffer[1024];

  while(1)
  {
    memset(&buffer, 0, sizeof(buffer));
    ipv4_header_s ipv4_header = {0};
    recv(sockfd, &buffer, sizeof(buffer), 0);

    unsigned int computed_checksum = 0;
    uint32_t host_word = ntohl(buffer[0]);
    computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
    ipv4_header.version         = (host_word>>28);
    ipv4_header.ihl             = (host_word>>24);
    ipv4_header.dscp            = (host_word>>18);
    ipv4_header.ecn             = (host_word>>16);
    ipv4_header.total_length    = (host_word);
    host_word = ntohl(buffer[1]);
    computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
    ipv4_header.identification  = (host_word>>16);
    ipv4_header.flags           = (host_word>>13);
    ipv4_header.fragment_offset = (host_word);
    host_word = ntohl(buffer[2]);
    computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
    ipv4_header.ttl             = (host_word>>24);
    ipv4_header.protocol        = (host_word>>16);
    ipv4_header.checksum        = (host_word);
    host_word = ntohl(buffer[3]);
    computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
    ipv4_header.source_ip       = host_word;
    host_word = ntohl(buffer[4]);
    computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
    ipv4_header.dest_ip         = host_word;

    unsigned int i;
    for(i = 5; i < ipv4_header.ihl; i++)
    {
      host_word = ntohl(buffer[i]);
      computed_checksum += (host_word & 0xFFFF) + (host_word>>16);
      ipv4_header.options[i] = host_word;
    }

    computed_checksum = (computed_checksum & 0xFFFF) + (computed_checksum>>16);

    printf("source 0x%x destination 0x%x identification 0x%x checksum 0x%x computed_checksum 0x%x\n", 
            ipv4_header.source_ip, ipv4_header.dest_ip, ipv4_header.identification, ipv4_header.checksum, computed_checksum);
  }
}
