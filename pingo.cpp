#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "icmp.hpp"
#include "ipv4.hpp"

/* ms*1000us/ms*1000ns/us */
#define MS_TO_NANOSEC(ms) (ms*1000*1000)

const unsigned int ping_block_size = 64;
const unsigned int ping_blocks = 4;
const uint32_t dest_base_address = (10<<24) | (73<<16) | (68<<8) | 0;

typedef struct 
{
  uint32_t        dest_address;
  struct timespec request_time;
} pingo_payload_t;


/* a-b=diff, returns false if input is null or b > a */
bool diff_time_spec(const struct timespec * a, const struct timespec * b, struct timespec * diff)
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

#define IP_STRING_SIZE 16
inline void ip_string(uint32_t address, char* buffer, size_t buffer_size)
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
  int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  unsigned int i,j;
  struct sockaddr_in write_addr = {0};
  unsigned int packet_id = 0;
  icmp_packet_meta_s icmp_packet_meta;
  pingo_payload_t pingo_payload = {0};
  struct timespec ping_block_cooldown = {0};
  uint32_t dest_address = dest_base_address;
  char ip_string_buffer[IP_STRING_SIZE];

  ipv4_word_t buffer[IPV4_MAX_PACKET_SIZE_WORDS];

  ping_block_cooldown.tv_sec = 0;
  ping_block_cooldown.tv_nsec = MS_TO_NANOSEC(250);

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

  for(i = 0; i < ping_blocks; i++)
  {
    for(j = 0; j < ping_block_size; j++)
    {
      memset(&icmp_packet_meta, 0, sizeof(icmp_packet_meta_s));

      icmp_packet_meta.header.type = ICMP_TYPE_ECHO_REQUEST;
      icmp_packet_meta.header.code = ICMP_CODE_ZERO;
      icmp_packet_meta.header.rest_of_header.id_seq_num.identifier      = 0xEDED;
      icmp_packet_meta.header.rest_of_header.id_seq_num.sequence_number = packet_id;
      icmp_packet_meta.header_valid = true;
      icmp_packet_meta.payload = (icmp_buffer_t*) &pingo_payload;
      icmp_packet_meta.payload_size = sizeof(pingo_payload_t);

      pingo_payload.dest_address = dest_address;
      clock_gettime(CLOCK_MONOTONIC_COARSE, &pingo_payload.request_time);

      ssize_t icmp_packet_size = encode_icmp_packet(&icmp_packet_meta, (icmp_buffer_t*) buffer, sizeof(buffer));
      ip_string(dest_address, ip_string_buffer, sizeof(ip_string_buffer));
      printf("ICMP Packet %u to %s size %lu\n", packet_id, ip_string_buffer, icmp_packet_size);

      write_addr = {0};
      write_addr.sin_family      = AF_INET;
      write_addr.sin_port        = htons(IPPROTO_ICMP);
      write_addr.sin_addr.s_addr = htonl(pingo_payload.dest_address);
      if(icmp_packet_size != sendto(sockfd, buffer, icmp_packet_size, 0, (sockaddr*)&write_addr, sizeof(write_addr)))
      {
        fprintf(stderr, "Failed to send to socket.  errno %u: %s\n", errno, strerror( errno));
        exit(1);
      }
      packet_id++;
      dest_address++;
   }
   nanosleep(&ping_block_cooldown,nullptr);
  }
  close(sockfd);
  return nullptr;
}

void *recv_thread_f(void*)
{
  int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  unsigned int i,j;
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

  for(i = 0; i < ping_blocks; i++)
  {
    for(j = 0; j < ping_block_size; j++)
    {
      memset(&buffer, 0, sizeof(buffer));

      recv_bytes = recv(sockfd, &buffer, sizeof(buffer), 0);
      clock_gettime(CLOCK_MONOTONIC_COARSE, &ping_reply_time);

      if(-1 == recv_bytes)
      {
        switch(errno)
        {
          case EWOULDBLOCK:
          {
            printf("Waiting for packets. Timeouts %u\n", recv_timeouts++);
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

        // printf("IPv4 valid %u src 0x%x dest 0x%x id 0x%x total_length %u ttl %u\n", 
        //         ipv4_packet_meta.header_valid,
        //         ipv4_packet_meta.header.source_ip, 
        //         ipv4_packet_meta.header.dest_ip, 
        //         ipv4_packet_meta.header.identification,
        //         ipv4_packet_meta.header.total_length,
        //         ipv4_packet_meta.header.ttl);

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
  clock_gettime(CLOCK_MONOTONIC_COARSE, &loop_start);
  pthread_create( &send_thread, NULL, send_thread_f, nullptr);

  pthread_join( send_thread, NULL);
  pthread_join( recv_thread, NULL); 

  clock_gettime(CLOCK_MONOTONIC_COARSE, &loop_end);
  clock_getres(CLOCK_MONOTONIC_COARSE, &clock_resolution);

  printf("Loop start: %lu.%09lu\n", loop_start.tv_sec, loop_start.tv_nsec);
  printf("Loop end:   %lu.%09lu\n", loop_end.tv_sec, loop_end.tv_nsec);
  printf("Resolution: %lu.%09lu\n", clock_resolution.tv_sec, clock_resolution.tv_nsec);
  diff_time_spec(&loop_end, &loop_start, &time_diff);
  printf("Diff: %lu.%09lu\n", time_diff.tv_sec, time_diff.tv_nsec);
}
