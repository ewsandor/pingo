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

  ipv4_word_t buffer[1024];

  while(1)
  {
    memset(&buffer, 0, sizeof(buffer));

    ipv4_packet_meta_s ipv4_packet_meta;
    icmp_packet_meta_s icmp_packet_meta;

    recv(sockfd, &buffer, sizeof(buffer), 0);

    ipv4_packet_meta = parse_ipv4_packet(buffer, sizeof(buffer));
    icmp_packet_meta = parse_icmp_packet(&ipv4_packet_meta.payload);

    printf("valid %u src 0x%x dest 0x%x id 0x%x\n", 
            ipv4_packet_meta.header_valid,
            ipv4_packet_meta.header.source_ip, 
            ipv4_packet_meta.header.dest_ip, 
            ipv4_packet_meta.header.identification);
  }
}
