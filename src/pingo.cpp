#include <assert.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "icmp.hpp"
#include "ipv4.hpp"
#include "ping_block.hpp"
#include "ping_logger.hpp"
#include "pingo.hpp"

using namespace sandor_laboratories::pingo;


pthread_mutex_t exit_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  exit_cond  = PTHREAD_COND_INITIALIZER;
unsigned int exit_block_mask = 0;
void sandor_laboratories::pingo::safe_exit(int status)
{
  int mutex_code, cond_code;
  struct timespec exit_timeout_abs;
  clock_gettime(CLOCK_REALTIME, &exit_timeout_abs);
  exit_timeout_abs.tv_sec += 5;

  mutex_code = pthread_mutex_timedlock(&exit_mutex, &exit_timeout_abs);
  if(mutex_code)
  {
    fprintf(stderr, "UNSAFE EXIT! Failed to lock exit mutex.  mutex_code %d %s\n", mutex_code, strerror(mutex_code));
    exit(1);
  }
  else
  {
    while(exit_block_mask)
    {
      fprintf(((0 == status)?stdout:stderr), "Waiting for safe exit.  Exit block mask 0x%x\n", exit_block_mask);
      cond_code = pthread_cond_timedwait(&exit_cond, &exit_mutex, &exit_timeout_abs);
      if(cond_code)
      {
        fprintf(stderr, "UNSAFE EXIT! Failed to meet exit condition.  cond_code %d - %s\n", cond_code, strerror(cond_code));
        exit(1);
      }
    }
    fprintf(((0 == status)?stdout:stderr), "Exiting safely with status %d.\n", status);
    exit(status);
    assert(0 == pthread_mutex_unlock(&exit_mutex));
  }
}
void sandor_laboratories::pingo::block_exit(exit_block_reason_e reason)
{
  assert(0 == pthread_mutex_lock(&exit_mutex));
  exit_block_mask |= (1<<reason);
  pthread_cond_broadcast(&exit_cond);
  assert(0 == pthread_mutex_unlock(&exit_mutex));
}
void sandor_laboratories::pingo::unblock_exit(exit_block_reason_e reason)
{
  assert(0 == pthread_mutex_lock(&exit_mutex));
  exit_block_mask &= ~(1<<reason);
  pthread_cond_broadcast(&exit_cond);
  assert(0 == pthread_mutex_unlock(&exit_mutex));
}


//const uint32_t dest_base_address = (10<<24) | (73<<16) | (68<<8) | 0;
//const uint32_t dest_base_address = (209<<24) | (131<<16) | (237<<8) | 0;
const uint32_t dest_base_address = (209<<24) | (131<<16) | (0<<8) | 0;

bool sandor_laboratories::pingo::timespec_valid(const struct timespec * test_timespec)
{
  bool ret_val = false;

  if(test_timespec)
  {
    ret_val = ( (test_timespec->tv_sec >= 0) &&  
                (test_timespec->tv_nsec >= 0) && 
                (test_timespec->tv_nsec < 1000000000) );
  }

  return ret_val;
}

/* a-b=diff, returns false if input is null or b > a */
bool sandor_laboratories::pingo::diff_timespec(const struct timespec * a, const struct timespec * b, struct timespec * diff)
{
  bool ret_val = false;
  
  if(diff)
  {
    memset(diff, 0, sizeof(*diff));

    if(a && b)
    {
      if( (a->tv_sec >  b->tv_sec) || 
          ((a->tv_sec == b->tv_sec) && (a->tv_nsec >= b->tv_nsec)))
      {
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
    }
    else
    {
      fprintf(stderr, "Null diff_timespec input. a %p b %p", a, b);
    }
  }
  else
  {
    fprintf(stderr, "Null diff_timespec input. diff %p", diff);
  }

  return ret_val;
}

void sandor_laboratories::pingo::ip_string(uint32_t address, char* buffer, size_t buffer_size, char deliminator, bool leading_zero)
{
  if(buffer)
  {
    if('.'==deliminator)
    {
      if(leading_zero)
      {
        snprintf(buffer, buffer_size, "%03u.%03u.%03u.%03u",
          ((address>>24) & 0xFF),
          ((address>>16) & 0xFF),
          ((address>>8)  & 0xFF),
          ((address)     & 0xFF));
      }
      else
      {
        snprintf(buffer, buffer_size, "%u.%u.%u.%u",
          ((address>>24) & 0xFF),
          ((address>>16) & 0xFF),
          ((address>>8)  & 0xFF),
          ((address)     & 0xFF));
      }
    }
    else
    {
      if(leading_zero)
      {
        snprintf(buffer, buffer_size, "%03u%c%03u%c%03u%c%03u",
          ((address>>24) & 0xFF),
          deliminator,
          ((address>>16) & 0xFF),
          deliminator,
          ((address>>8)  & 0xFF),
          deliminator,
          ((address)     & 0xFF));
      }
      else
      {
        snprintf(buffer, buffer_size, "%u%c%u%c%u%c%u",
          ((address>>24) & 0xFF),
          deliminator,
          ((address>>16) & 0xFF),
          deliminator,
          ((address>>8)  & 0xFF),
          deliminator,
          ((address)     & 0xFF));
      }
    }
  }
}

void *writer_thread_f(void* arg)
{
  ping_logger_c *ping_logger = (ping_logger_c*) arg;
  ping_block_c  *ping_block;
  unsigned int ping_block_counter = 0;
  const struct timespec soak_time = {.tv_sec = 60, .tv_nsec = 0};
  struct timespec remaining_soak_time, time_since_dispatch, dispatch_time;
  char ip_string_buffer[IP_STRING_SIZE];

  while(1)
  {
    printf("Waiting for ping block.\n");
    ping_logger->wait_for_ping_block();
    printf("%u ping blocks registered.\n", ping_logger->get_num_ping_blocks());
    ping_block = ping_logger->peek_ping_block();
    ping_block->wait_dispatch_done();
    dispatch_time = ping_block->get_dispatch_time();
    ip_string(ping_block->get_first_address(), ip_string_buffer, sizeof(ip_string_buffer));
    printf("Ping block %u starting at %s with %u IPs dispatched in %lu.%03lus.\n", 
      ping_block_counter, ip_string_buffer, ping_block->get_address_count(), dispatch_time.tv_sec, NANOSEC_TO_MS(dispatch_time.tv_nsec));
    time_since_dispatch = ping_block->time_since_dispatch();
    if(diff_timespec(&soak_time, &time_since_dispatch, &remaining_soak_time))
    {
      printf("Soaking for %lu.%09lu more seconds.\n", remaining_soak_time.tv_sec, remaining_soak_time.tv_nsec);
      nanosleep(&remaining_soak_time, nullptr);
    }
    assert(ping_block == ping_logger->pop_ping_block());
    time_since_dispatch = ping_block->time_since_dispatch();
    printf("Soaked %lu.%09lu seconds.\n", time_since_dispatch.tv_sec, time_since_dispatch.tv_nsec);
    printf("Deleted ping block.\n");
    delete ping_block;
    ping_block_counter++;
  }

  return nullptr;
}

void *send_thread_f(void* arg)
{
  ping_block_config_s    ping_block_config;
  ping_block_c          *ping_block;
  ping_logger_c         *ping_logger = (ping_logger_c*) arg;
  uint32_t               ping_block_first_address = dest_base_address;
  const struct timespec  cool_down = {.tv_sec = 0, .tv_nsec = 0};

  ping_block_c::init_config(&ping_block_config); 
  ping_block_config.verbose = false;

  while(1)
  {
    ping_block = new ping_block_c(ping_block_first_address, 65536, &ping_block_config);
    ping_block_first_address = ping_block->get_last_address();
    ping_logger->push_ping_block(ping_block);
    ping_block->dispatch();
    nanosleep(&cool_down, nullptr);
  }

  return nullptr;
}

void *recv_thread_f(void*)
{
  int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  ipv4_packet_meta_s ipv4_packet_meta;
  icmp_packet_meta_s icmp_packet_meta;
  pingo_payload_t pingo_payload;
  struct timespec ping_reply_time;
  struct timespec time_diff;
  char ip_string_buffer_a[IP_STRING_SIZE], ip_string_buffer_b[IP_STRING_SIZE];
  struct timeval recv_timeout;
  unsigned int recv_timeouts = 0;
  ssize_t recv_bytes;
  const bool verbose = false;
  ping_logger_entry_s log_entry;
  struct sockaddr_in src_addr;
  socklen_t addrlen;
  ipv4_word_t buffer[IPV4_MAX_PACKET_SIZE_WORDS];

  memset(&pingo_payload, 0, sizeof(pingo_payload));
  memset(&ping_reply_time, 0, sizeof(ping_reply_time));
  memset(&time_diff, 0, sizeof(time_diff));
  memset(&log_entry, 0, sizeof(log_entry));

  if(sockfd == -1)
  {
    switch(errno)
    {
      case EPERM:
      {
        fprintf(stderr, "No permission to open socket.\n");
        safe_exit(126);
        break;
      }
      default:
      {
        fprintf(stderr, "Failed to open socket.  errno %u: %s\n", errno, strerror(errno));
        safe_exit(1);
        break;
      }
    }
  }

  recv_timeout.tv_sec  = 1;
  recv_timeout.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&recv_timeout, sizeof(recv_timeout));

  while(1)
  {
    memset(&buffer, 0, sizeof(buffer));

    recv_bytes = recvfrom(sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr*) &src_addr, &addrlen);
    assert(sizeof(struct sockaddr_in ) == addrlen);
    get_time(&ping_reply_time);

    if(-1 == recv_bytes)
    {
      switch(errno)
      {
        case EWOULDBLOCK:
        {
          if(verbose)
          {
            printf("Waiting for packets. Timeouts %u\n", ++recv_timeouts);
          }
          break;
        }
        default:
        {
          fprintf(stderr, "Failed to receive from socket.  errno %u: %s\n", errno, strerror(errno));
          safe_exit(1);
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
        ip_string(ipv4_packet_meta.header.source_ip, ip_string_buffer_a, sizeof(ip_string_buffer_a));
        if(icmp_packet_meta.header_valid)
        {
          switch(icmp_packet_meta.header.type)
          {
            case ICMP_TYPE_ECHO_REPLY:
            {
              if((icmp_packet_meta.payload_size == sizeof(pingo_payload_t)))
              {
                pingo_payload = *((pingo_payload_t*)icmp_packet_meta.payload);

                if( (ICMP_IDENTIFIER == icmp_packet_meta.header.rest_of_header.id_seq_num.identifier) &&
                    (ipv4_packet_meta.header.source_ip == pingo_payload.dest_address) &&
                    (timespec_valid(&pingo_payload.request_time)) )
                {
                  diff_timespec(&ping_reply_time, &pingo_payload.request_time, &time_diff);

                  if(verbose)
                  {
                    ip_string(ipv4_packet_meta.header.source_ip, ip_string_buffer_a, sizeof(ip_string_buffer_a));
                    printf("Ping reply from %s in %lu.%09lus\n", ip_string_buffer_a, time_diff.tv_sec, time_diff.tv_nsec);
                  }
                }
                else
                {
                  ip_string(ipv4_packet_meta.header.source_ip, ip_string_buffer_a, sizeof(ip_string_buffer_a));
                  ip_string(pingo_payload.dest_address, ip_string_buffer_b, sizeof(ip_string_buffer_b));
                  fprintf(stderr, "Invalid echo reply payload from %s.  identifier 0x%x sequence %u pingo_dest_address %s pingo_request_time %lu.%09lus\n", 
                          ip_string_buffer_a, 
                          icmp_packet_meta.header.rest_of_header.id_seq_num.identifier, 
                          icmp_packet_meta.header.rest_of_header.id_seq_num.sequence_number, 
                          ip_string_buffer_b,
                          pingo_payload.request_time.tv_sec,
                          pingo_payload.request_time.tv_nsec);
                }
              }
              else
              {
                ip_string(ipv4_packet_meta.header.source_ip, ip_string_buffer_a, sizeof(ip_string_buffer_a));
                fprintf(stderr, "Invalid echo reply payload size from %s.  payload_size %lu expected %lu\n", 
                        ip_string_buffer_a, icmp_packet_meta.payload_size, sizeof(pingo_payload_t));
              }
              break;
            }
            default:
            {

              if(verbose)
              {
                printf("icmp valid %u from %s type %u code %u id %u seq_num %u payload_size %lu\n", 
                        icmp_packet_meta.header_valid,
                        ip_string_buffer_a,
                        icmp_packet_meta.header.type, 
                        icmp_packet_meta.header.code, 
                        icmp_packet_meta.header.rest_of_header.id_seq_num.identifier,
                        icmp_packet_meta.header.rest_of_header.id_seq_num.sequence_number,
                        icmp_packet_meta.payload_size);
              }
            }
          }
        }
        else
        {
          ip_string(ntohl(src_addr.sin_addr.s_addr), ip_string_buffer_a, sizeof(ip_string_buffer_a));
          fprintf(stderr, "Invalid packet from %s.  ICMP header valid %u\n", 
            ip_string_buffer_a, icmp_packet_meta.header_valid);
        }
      }
      else
      {
        ip_string(ntohl(src_addr.sin_addr.s_addr), ip_string_buffer_a, sizeof(ip_string_buffer_a));
        fprintf(stderr, "Invalid packet from %s.  IPv4 header valid %u\n", 
          ip_string_buffer_a, ipv4_packet_meta.header_valid);
      }
    }
  }
  close(sockfd);
  return nullptr;
}

void signal_handler(int signal) 
{
  switch(signal)
  {
    default:
    {
      printf("Received signal %d\n", signal);
      safe_exit(128+signal);
    }
  }
} 

int main(int argc, char *argv[])
{
  ping_logger_c ping_logger;
  pthread_t writer_thread, recv_thread, send_thread;

  UNUSED(argc);
  UNUSED(argv);

  signal(SIGINT,  signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGQUIT, signal_handler);

  pthread_create(&writer_thread, NULL, writer_thread_f, &ping_logger);
  pthread_create(&recv_thread,   NULL, recv_thread_f,   nullptr);
  pthread_create(&send_thread,   NULL, send_thread_f,   &ping_logger);

  while('q' != getchar()) {}
  
  safe_exit(0);
}