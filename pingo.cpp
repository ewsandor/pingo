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
    ipv4_header_s ipv4_header;
    recv(sockfd, &buffer, sizeof(buffer), 0);

    parse_ipv4_header(buffer, sizeof(buffer), &ipv4_header);

    printf("src 0x%x dest 0x%x id 0x%x\n", 
            ipv4_header.source_ip, ipv4_header.dest_ip, ipv4_header.identification);
  }
}
