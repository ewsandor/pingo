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
    .identifier      = ICMP_IDENTIFIER,
    .fixed_sequence_number = false,
    .sequence_number = 0,
    .send_attempts   = 5,
    .excluded_ip_list = nullptr,
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

  dispatch_started = false;
  fully_dispatched = false;
  memset(&dispatch_start_time, 0, sizeof(dispatch_start_time));
  memset(&dispatch_done_time,  0, sizeof(dispatch_done_time));
  memset(&dispatch_time,       0, sizeof(dispatch_time));

  entry = (ping_block_entry_s*) calloc(address_count, sizeof(ping_block_entry_s));

  for(i = 0; i < address_count; i++)
  {
    entry[i].ping_time = PINGO_BLOCK_PING_TIME_NO_RESPONSE;
  }

  if(config.excluded_ip_list)
  {
    for(ping_block_excluded_ip_list_t::iterator it = config.excluded_ip_list->begin(); it != config.excluded_ip_list->end(); it++)
    {
      if( ((it->ip | (~it->subnet_mask)) >= get_first_address()) &&
          ((it->ip & ( it->subnet_mask)) <  get_last_address()) )
      {
        const ping_block_excluded_ip_s excluded_ip = 
          {
            .ip          = (it->ip & it->subnet_mask),
            .subnet_mask = it->subnet_mask, 
          };
        excluded_ip_list.push_back(excluded_ip);

        char ip_string_buffer[IP_STRING_SIZE];
        char subnet_string_buffer[IP_STRING_SIZE];
        ip_string(excluded_ip.ip, ip_string_buffer, sizeof(ip_string_buffer));
        ip_string(excluded_ip.subnet_mask, subnet_string_buffer, sizeof(ip_string_buffer));
        printf("Excluding IP %s with subnet mask %s from ping block.\n", ip_string_buffer, subnet_string_buffer);
      }
    }
  }

  unlock();
}

ping_block_c::ping_block_c(uint32_t first_address, unsigned int address_count)
  : ping_block_c(first_address, address_count, &default_ping_block_config) {}

ping_block_c::~ping_block_c()
{
  lock();

  free(entry);

  unlock();

  assert(0 == pthread_mutex_destroy(&mutex));
}


bool ping_block_c::log_ping_time(uint32_t address, reply_time_t reply_delay)
{
  bool ret_val = false;
  ping_block_entry_s* log_entry = nullptr;

  if( (address >= get_first_address()) &&
      ((address-get_first_address()) < get_address_count()))
  {
    ret_val = true;

    lock();

    log_entry = &entry[(address-get_first_address())];

    log_entry->reply_valid = true;
    
    log_entry->ping_time = 
      (reply_delay<PINGO_BLOCK_PING_TIME_NO_RESPONSE)?
       reply_delay:PINGO_BLOCK_PING_TIME_NO_RESPONSE;

    unlock();
  }

  return ret_val;
}

bool ping_block_c::get_ping_block_entry(uint32_t address, ping_block_entry_s* ret_entry)
{
  bool ret_val = true;
  if(ret_entry)
  {
    if( (address >= get_first_address()) &&
        ((address-get_first_address()) < get_address_count()))
    {
      lock();
      *ret_entry = entry[(address-get_first_address())];
      unlock();
    }
    else
    {
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null ret_entry pointer passed\n");
    ret_val = false;
  }

  return ret_val;
}

bool ping_block_c::dispatch()
{
  bool ret_val = false;

  int                 sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  unsigned int        i, batch_index = 0, packet_id = 0;
  icmp_packet_meta_s  icmp_packet_meta;
  pingo_payload_t     pingo_payload;
  uint32_t            dest_address = get_first_address();
  struct sockaddr_in  send_sockaddr;
  struct timespec     temp_time;
  char                ip_string_buffer[IP_STRING_SIZE];
  ipv4_word_t         buffer[IPV4_MAX_PACKET_SIZE_WORDS];
  unsigned int        remaining_attempts;

  get_time(&temp_time);

  if(!is_dispatch_started())
  {
    dispatch_started = true;
    dispatch_start_time = temp_time;

    memset(&pingo_payload, 0, sizeof(pingo_payload));

    memset(&send_sockaddr, 0, sizeof(send_sockaddr));
    send_sockaddr.sin_family = AF_INET;
    send_sockaddr.sin_port   = htons(IPPROTO_ICMP);

    memset(&icmp_packet_meta, 0, sizeof(icmp_packet_meta_s));
    icmp_packet_meta.header.type = ICMP_TYPE_ECHO_REQUEST;
    icmp_packet_meta.header.code = ICMP_CODE_ZERO;
    icmp_packet_meta.header.rest_of_header.id_seq_num.identifier = config.identifier;
    icmp_packet_meta.header_valid = true;
    icmp_packet_meta.payload = (icmp_buffer_t*) &pingo_payload;
    icmp_packet_meta.payload_size = sizeof(pingo_payload_t);

    if(sockfd == -1)
    {
      switch(errno)
      {
        case EPERM:
        {
          fprintf(stderr, "No permission to open socket for ping block dispatch.\n");
          safe_exit(126);
          break;
        }
        default:
        {
          fprintf(stderr, "Failed to open socket for ping block dispatch.  errno %u: %s\n", errno, strerror(errno));
          safe_exit(1);
          break;
        }
      }
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
          if(!exclude_ip_address(dest_address))
          {
            remaining_attempts = config.send_attempts;
            send_sockaddr.sin_addr.s_addr = htonl(dest_address);
            icmp_packet_meta.header.checksum = 0;
            icmp_packet_meta.header.rest_of_header.id_seq_num.sequence_number = 
              (config.fixed_sequence_number?config.sequence_number:packet_id);
            pingo_payload.dest_address = dest_address;
            get_time(&pingo_payload.request_time);

            ssize_t icmp_packet_size = encode_icmp_packet(&icmp_packet_meta, (icmp_buffer_t*) buffer, sizeof(buffer));

            while((remaining_attempts > 0) &&
                  (icmp_packet_size != sendto(sockfd, buffer, icmp_packet_size, 0, (sockaddr*)&send_sockaddr, sizeof(send_sockaddr))))
            {
              ip_string(dest_address, ip_string_buffer, sizeof(ip_string_buffer));
              fprintf(stderr, "Failed to send ping for IP %s to socket.  errno %u: %s\n", ip_string_buffer, errno, strerror(errno));
              remaining_attempts--;
              if(0 == remaining_attempts)
              {
                fprintf(stderr, "Aborting further attempts to send ping.\n");
                lock();
                assert((dest_address >= get_first_address()) && ((dest_address-get_first_address()) < get_address_count()));
                entry[(dest_address-get_first_address())] = 
                  {
                    .reply_valid = false,
                    .ping_time   = PINGO_BLOCK_PING_TIME_NO_RESPONSE,
                    .skip_reason = PING_BLOCK_IP_SKIP_REASON_SOCKET_ERROR,
                    .skip_errno  = errno,
                  };
                unlock();
              }
              else
              {
                fprintf(stderr, "Reattempting to send ping in 1s.  %u attempts remaining\n", remaining_attempts);
                sleep(1);
              }
            }
          }
          else
          {
            lock();
            assert((dest_address >= get_first_address()) && ((dest_address-get_first_address()) < get_address_count()));
            entry[(dest_address-get_first_address())] = 
              {
                .reply_valid = false,
                .ping_time   = PINGO_BLOCK_PING_TIME_NO_RESPONSE,
                .skip_reason = PING_BLOCK_IP_SKIP_REASON_EXCLUDE_LIST,
                .skip_errno  = -1,
              };
            unlock();
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
      get_time(&temp_time);
      ret_val = true;
        
      if(0 != close(sockfd))
      {
        ip_string(get_first_address(), ip_string_buffer, sizeof(ip_string_buffer));
        fprintf(stderr, "Failed to close socket for ping block.  First address %s address count %u.  errno %u: %s\n", 
          ip_string_buffer, get_address_count(), errno, strerror(errno));
        ret_val = false;
      }

      lock();
      dispatch_done_time = temp_time;
      diff_timespec(&dispatch_done_time, &dispatch_start_time, &dispatch_time);
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
  }
  else
  {
    ip_string(dest_address, ip_string_buffer, sizeof(ip_string_buffer));
    fprintf(stderr, "Dispatch for ping block starting at IP %s already started.\n", ip_string_buffer);
  }

  return ret_val;
}
bool ping_block_c::is_dispatch_started()
{
  bool ret_val = false;

  lock();
  ret_val = dispatch_started;
  unlock();

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
struct timespec ping_block_c::get_dispatch_time()
{
  struct timespec ret_val;

  lock();
  ret_val = dispatch_time;
  unlock();

  return ret_val;
}
struct timespec ping_block_c::get_dispatch_start_time()
{
  struct timespec ret_val;

  lock();
  ret_val = dispatch_start_time;
  unlock();

  return ret_val;
}

struct timespec ping_block_c::get_dispatch_done_time()
{
  struct timespec ret_val;
  lock();
  ret_val = dispatch_done_time;
  unlock();

  return ret_val;
}

struct timespec ping_block_c::time_since_dispatch()
{
  struct timespec dispatch_done_time_copy, time_now, ret_val;

  memset(&ret_val, 0, sizeof(ret_val));

  if(is_fully_dispatched())
  {
    dispatch_done_time_copy = get_dispatch_done_time();
    get_time(&time_now);
    diff_timespec(&time_now, &dispatch_done_time_copy, &ret_val);
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

ping_block_stats_s ping_block_c::get_stats()
{
  ping_block_stats_s stats;
  unsigned int i;

  memset(&stats, 0, sizeof(stats));
  stats.min_reply_time  = PINGO_BLOCK_PING_TIME_NO_RESPONSE;
  stats.mean_reply_time = PINGO_BLOCK_PING_TIME_NO_RESPONSE;
  stats.max_reply_time  = PINGO_BLOCK_PING_TIME_NO_RESPONSE;

  lock();

  for(i = 0; i < get_address_count(); i++)
  {
    if(entry[i].reply_valid)
    {
      stats.valid_replies++;

      if((entry[i].ping_time < stats.min_reply_time) || (PINGO_BLOCK_PING_TIME_NO_RESPONSE == stats.min_reply_time))
      {
        stats.min_reply_time = entry[i].ping_time;
      }
      if((entry[i].ping_time > stats.max_reply_time) || (PINGO_BLOCK_PING_TIME_NO_RESPONSE == stats.max_reply_time))
      {
        stats.max_reply_time = entry[i].ping_time;
      }

      stats.mean_reply_time += entry[i].ping_time;
    }
    else if(entry[i].skip_reason > 0)
    {
      stats.skipped_pings++;
    }
  }

  unlock();

  if(stats.valid_replies)
  {
    stats.mean_reply_time /= stats.valid_replies;
  }
  else
  {
    stats.mean_reply_time = PINGO_BLOCK_PING_TIME_NO_RESPONSE;
  }

  return stats;
}

inline bool ping_block_c::exclude_ip_address(const uint32_t ip)
{
  bool ret_val = false;

  for(ping_block_excluded_ip_list_t::iterator it = excluded_ip_list.begin(); it != excluded_ip_list.end(); it++)
  {
    if((ip & it->subnet_mask) == it->ip)
    {
      ret_val = true;
      break;
    }
  }

  return ret_val;
}