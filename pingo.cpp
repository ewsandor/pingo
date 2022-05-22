#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include "icmp.hpp"
#include "ipv4.hpp"
#include "ping_block.hpp"
#include "pingo.hpp"

using namespace sandor_laboratories::pingo;

//const uint32_t dest_base_address = (10<<24) | (73<<16) | (68<<8) | 0;
//const uint32_t dest_base_address = (209<<24) | (131<<16) | (237<<8) | 0;
const uint32_t dest_base_address = (209<<24) | (131<<16) | (0<<8) | 0;

/* a-b=diff, returns false if input is null or b > a */
bool sandor_laboratories::pingo::diff_time_spec(const struct timespec * a, const struct timespec * b, struct timespec * diff)
{
  bool ret_val = false;
  
  if((a && b && diff) &&
     ((a->tv_sec >  b->tv_sec) || 
      ((a->tv_sec == b->tv_sec) && (a->tv_nsec >= b->tv_nsec))))
  {
    *diff = {0};
    diff->tv_sec  = a->tv_sec-b->tv_sec;
    diff->tv_nsec = a->tv_nsec;
    if(b->tv_nsec > a->tv_nsec)
    {
      diff->tv_nsec += 1000000000;
      diff->tv_sec--;
    }
    diff->tv_nsec -= b->tv_nsec;

    ret_val = true;
  }

  return ret_val;
}

void sandor_laboratories::pingo::ip_string(uint32_t address, char* buffer, size_t buffer_size)
{
  if(buffer)
  {
    snprintf(buffer, buffer_size, "%u.%u.%u.%u",
    ((address>>24) & 0xFF),
    ((address>>16) & 0xFF),
    ((address>>8) & 0xFF),
    ((address) & 0xFF));
  }
}

void *send_thread_f(void*)
{
  ping_block_c ping_block(dest_base_address, 256*256);

  ping_block.dispatch();

  return nullptr;
}

void *recv_thread_f(void*)
{
  int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  ipv4_packet_meta_s ipv4_packet_meta;
  icmp_packet_meta_s icmp_packet_meta;
  pingo_payload_t pingo_payload = {0};
  struct timespec ping_reply_time = {0};
  struct timespec time_diff = {0};
  char ip_string_buffer[IP_STRING_SIZE];
  struct timeval recv_timeout;
  unsigned int recv_timeouts = 0;
  ssize_t recv_bytes;

  ipv4_word_t buffer[IPV4_MAX_PACKET_SIZE_WORDS];

  if(sockfd == -1)
  {
    switch(errno)
    {
      case EPERM:
      {
        fprintf(stderr, "No permission to open socket.\n");
        break;
      }
      default:
      {
        fprintf(stderr, "Failed to open socket.  errno %u: %s\n", errno, strerror(errno));
        break;
      }
    }
    exit(1);
  }

  recv_timeout.tv_sec  = 1;
  recv_timeout.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&recv_timeout, sizeof(recv_timeout));

  while(1)
  {
    memset(&buffer, 0, sizeof(buffer));

    recv_bytes = recv(sockfd, &buffer, sizeof(buffer), 0);
    get_time(&ping_reply_time);

    if(-1 == recv_bytes)
    {
      switch(errno)
      {
        case EWOULDBLOCK:
        {
          printf("Waiting for packets. Timeouts %u\n", ++recv_timeouts);
          break;
        }
        default:
        {
          fprintf(stderr, "Failed to receive from socket.  errno %u: %s\n", errno, strerror(errno));
          exit(1);
          break;
        }
      }
    }
    else if(0 == recv_bytes)
    {
      printf("Empty packet.\n");
    }
    else
    {
      ipv4_packet_meta = parse_ipv4_packet(buffer, sizeof(buffer));

      if(ipv4_packet_meta.header_valid)
      {
        icmp_packet_meta = parse_icmp_packet(&ipv4_packet_meta.payload);
        ip_string(ipv4_packet_meta.header.source_ip, ip_string_buffer, sizeof(ip_string_buffer));
        printf("icmp valid %u from %s type %u code %u id %u seq_num %u payload_size %lu\n", 
                icmp_packet_meta.header_valid,
                ip_string_buffer,
                icmp_packet_meta.header.type, 
                icmp_packet_meta.header.code, 
                icmp_packet_meta.header.rest_of_header.id_seq_num.identifier,
                icmp_packet_meta.header.rest_of_header.id_seq_num.sequence_number,
                icmp_packet_meta.payload_size);
        if(icmp_packet_meta.header_valid && (icmp_packet_meta.payload_size == sizeof(pingo_payload_t)))
        {
          pingo_payload = *((pingo_payload_t*)icmp_packet_meta.payload);
          diff_time_spec(&ping_reply_time, &pingo_payload.request_time, &time_diff);
          printf("Ping time: %lu.%09lu\n", time_diff.tv_sec, time_diff.tv_nsec);
        }
      }
    }
  }
  close(sockfd);
  return nullptr;
}

int main(int argc, char *argv[])
{
  struct timespec clock_resolution = {0};
  struct timespec loop_start = {0};
  struct timespec loop_end = {0};
  struct timespec time_diff = {0};

  pthread_t send_thread, recv_thread;


  pthread_create( &recv_thread, NULL, recv_thread_f, nullptr);
  get_time(&loop_start);
  pthread_create( &send_thread, NULL, send_thread_f, nullptr);

  pthread_join( send_thread, NULL);

  get_time(&loop_end);
  get_time(&clock_resolution);

  printf("Loop start: %lu.%09lu\n", loop_start.tv_sec, loop_start.tv_nsec);
  printf("Loop end:   %lu.%09lu\n", loop_end.tv_sec, loop_end.tv_nsec);
  printf("Resolution: %lu.%09lu\n", clock_resolution.tv_sec, clock_resolution.tv_nsec);
  diff_time_spec(&loop_end, &loop_start, &time_diff);
  printf("Diff: %lu.%09lu\n", time_diff.tv_sec, time_diff.tv_nsec);

  while('q' != getchar()) {}
}
