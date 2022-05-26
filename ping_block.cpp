#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "icmp.hpp"
#include "ping_block.hpp"
#include "pingo.hpp"

using namespace sandor_laboratories::pingo;

static const ping_block_config_s default_ping_block_config = 
  {
    .verbose         = true,
    .ping_batch_size = 256,
    .ping_batch_cooldown = 
      {
        .tv_sec  = 0,
        .tv_nsec = MS_TO_NANOSEC(50),
      },
    .socket_ttl      = 255,
    .identifier      = 0xEdEd,
  };

void ping_block_c::init_config(ping_block_config_s* new_config)
{
  if(new_config)
  {
    *new_config = default_ping_block_config;
  }
}

inline void ping_block_c::lock()
{
  assert(0 == pthread_mutex_lock(&mutex));
}
inline void ping_block_c::unlock()
{
  assert(0 == pthread_mutex_unlock(&mutex));
}


ping_block_c::ping_block_c(uint32_t first_address, unsigned int address_count, const ping_block_config_s *init_config)
  : first_address(first_address), address_count(address_count), config(*init_config)
{
  unsigned int i;

  assert(0 == pthread_mutex_init(&mutex, NULL));

  lock();

  fully_dispatched = false;
  dispatch_done_time = {0};

  entry = (ping_block_entry_s*) calloc(address_count, sizeof(ping_block_entry_s));

  for(i = 0; i < address_count; i++)
  {
    entry[i].ping_time = PINGO_BLOCK_PING_TIME_NO_RESPONSE;
  }

  unlock();
}
ping_block_c::ping_block_c(uint32_t first_address, unsigned int address_count)
  : ping_block_c(first_address, address_count, &default_ping_block_config) {};

ping_block_c::~ping_block_c()
{
  lock();

  free(entry);

  unlock();

  assert(0 == pthread_mutex_destroy(&mutex));
}


bool ping_block_c::log_ping_time(uint32_t address, unsigned int ping_time)
{
  bool ret_val = false;
  ping_block_entry_s* log_entry = nullptr;

  if((address-get_first_address()) < get_address_count())
  {
    ret_val = true;

    lock();

    log_entry = &entry[(address-get_first_address())];
    
    log_entry->ping_time = 
      (ping_time<PINGO_BLOCK_PING_TIME_NO_RESPONSE)?
       ping_time:PINGO_BLOCK_PING_TIME_NO_RESPONSE;

    unlock();
  }

  return ret_val;
}

bool ping_block_c::dispatch()
{
  bool ret_val = false;

  int                 sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  unsigned int        i, batch_index = 0, packet_id = 0;
  icmp_packet_meta_s  icmp_packet_meta;
  pingo_payload_t     pingo_payload = {0};
  uint32_t            dest_address = get_first_address();
  struct sockaddr_in  send_sockaddr = {0};
  struct timespec     dispatch_done_time_temp;
  char                ip_string_buffer[IP_STRING_SIZE];
  ipv4_word_t         buffer[IPV4_MAX_PACKET_SIZE_WORDS];

  if(sockfd == -1)
  {
    fprintf(stderr, "Failed to open socket to dispatch ping block.  errno %u: %s\n", errno, strerror(errno));
  }
  else
  {
    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &config.socket_ttl, sizeof(config.socket_ttl));

    for(batch_index = 0; dest_address < get_last_address(); batch_index++)
    {
      if(config.verbose)
      {
        ip_string(dest_address, ip_string_buffer, sizeof(ip_string_buffer));
        printf("Ping batch %u.  %u IPs starting at IP %s\n",
              batch_index, config.ping_batch_size, ip_string_buffer);
      }

      for(i = 0; i < config.ping_batch_size; i++)
      {
        memset(&icmp_packet_meta, 0, sizeof(icmp_packet_meta_s));

        icmp_packet_meta.header.type = ICMP_TYPE_ECHO_REQUEST;
        icmp_packet_meta.header.code = ICMP_CODE_ZERO;
        icmp_packet_meta.header.rest_of_header.id_seq_num.identifier      = config.identifier;
        icmp_packet_meta.header.rest_of_header.id_seq_num.sequence_number = packet_id;
        icmp_packet_meta.header_valid = true;
        icmp_packet_meta.payload = (icmp_buffer_t*) &pingo_payload;
        icmp_packet_meta.payload_size = sizeof(pingo_payload_t);

        pingo_payload.dest_address = dest_address;
        get_time(&pingo_payload.request_time);

        ssize_t icmp_packet_size = encode_icmp_packet(&icmp_packet_meta, (icmp_buffer_t*) buffer, sizeof(buffer));

        send_sockaddr = {0};
        send_sockaddr.sin_family      = AF_INET;
        send_sockaddr.sin_port        = htons(IPPROTO_ICMP);
        send_sockaddr.sin_addr.s_addr = htonl(pingo_payload.dest_address);
        while(icmp_packet_size != sendto(sockfd, buffer, icmp_packet_size, 0, (sockaddr*)&send_sockaddr, sizeof(send_sockaddr)))
        {
          ip_string(dest_address, ip_string_buffer, sizeof(ip_string_buffer));
          fprintf(stderr, "Failed to send ping for IP %s to socket.  errno %u: %s\n", ip_string_buffer, errno, strerror(errno));
          switch(errno)
          {
            case ENOBUFS:
            {
              fprintf(stderr, "Retrying to send ping in 1s\n");
              sleep(1);
              break;
            }
            default:
            {
              safe_exit(1);
              break;
            }
          }
        }
        packet_id++;
        dest_address++;
        if(dest_address >= get_last_address())
        {
          break;
        }
      }
      if(dest_address < get_last_address())
      {
        nanosleep(&config.ping_batch_cooldown,nullptr);
      }
    }
    get_time(&dispatch_done_time_temp);
    ret_val = true;
      
    if(0 != close(sockfd))
    {
      ip_string(get_first_address(), ip_string_buffer, sizeof(ip_string_buffer));
      fprintf(stderr, "Failed to close socket for ping block.  First address %s address count %u.  errno %u: %s\n", 
        ip_string_buffer, get_address_count(), errno, strerror(errno));
      ret_val = false;
    }

    lock();
    dispatch_done_time = dispatch_done_time_temp;
    fully_dispatched = true;
    assert(0==pthread_cond_broadcast(&dispatch_done_cond));
    unlock();
 
    if(config.verbose)
    {
      ip_string(get_first_address(), ip_string_buffer, sizeof(ip_string_buffer));
      printf("Done dispatching ping block.  First address %s address count %u.\n", 
        ip_string_buffer, get_address_count());
    }
  }

  return ret_val;
}
bool ping_block_c::is_fully_dispatched()
{
  bool ret_val = false;

  lock();
  ret_val = fully_dispatched;
  unlock();

  return ret_val;
}

struct timespec ping_block_c::get_dispatch_done_time()
{
  struct timespec ret_val = {0};

  lock();
  if(fully_dispatched)
  {
    ret_val = dispatch_done_time;
  }
  unlock();

  return ret_val;
}

struct timespec ping_block_c::time_since_dispatch()
{
  struct timespec dispatch_done_time_copy, time_now, ret_val = {0};

  if(is_fully_dispatched())
  {
    dispatch_done_time_copy = get_dispatch_done_time();
    get_time(&time_now);
    diff_time_spec(&time_now, &dispatch_done_time_copy, &ret_val);
  }

  return ret_val;
}

void ping_block_c::wait_dispatch_done()
{
  lock();

  while(!fully_dispatched)
  {
    assert(0==pthread_cond_wait(&dispatch_done_cond, &mutex));
  }

  unlock();
}