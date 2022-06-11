#include <cassert>
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

#include "file.hpp"
#include "icmp.hpp"
#include "ipv4.hpp"
#include "ping_block.hpp"
#include "ping_logger.hpp"
#include "pingo.hpp"
#include "version.hpp"

#include "hilbert.hpp"

using namespace sandor_laboratories::pingo;

typedef enum
{
  PINGO_ARGUMENT_UNSPECIFIED,
  PINGO_ARGUMENT_VALID,
  PINGO_ARGUMENT_INVALID,
} pingo_argument_status_e;

typedef struct
{
  pingo_argument_status_e initial_ip_status;
  uint32_t                initial_ip;

  pingo_argument_status_e address_length_status;
  uint32_t                address_length;

  pingo_argument_status_e cooldown_status;
  uint32_t                cooldown;

} pingo_ping_block_arguments_s;

typedef struct
{
  pingo_argument_status_e directory_status;
  char                    directory[FILE_PATH_MAX_LENGTH];

  pingo_argument_status_e soak_timeout_status;
  unsigned int            soak_timeout;
} pingo_writer_arguments_s;


typedef struct 
{
  bool                         unexpected_arg;

  pingo_argument_status_e      help_request;
  pingo_argument_status_e      validate_status;

  pingo_ping_block_arguments_s ping_block_args;
  pingo_writer_arguments_s     writer_args;

} pingo_arguments_s;

const char *help_string = PROJECT_NAME " " PROJECT_VER " <" PROJECT_URL ">\n"
                          PROJECT_DESCRIPTION "\n\n"
                          "Options:\n"
                          "-c: Cooldown time in milliseconds between ping block batches\n"
                          "-d: Directory to write ping data\n"
                          "-i: Initial IP address to ping\n"
                          "-s: Size of ping blocks\n"
                          "-t: Ping block soaking Timeout\n"
                          "-v: Validate pingo files at directory and exit\n"
                          "-h: Display this Help text\n";

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


//const uint32_t dest_base_address = (10<<24) | (0<<16) | (0<<8) | 0;
//const uint32_t dest_base_address = (73<<24) | (0<<16) | (0<<8) | 0;
//const uint32_t dest_base_address = (209<<24) | (131<<16) | (0<<8) | 0;
const uint32_t dest_base_address = 0;

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

void *log_handler_thread_f(void* arg)
{
  ping_logger_c *ping_logger = (ping_logger_c*) arg;

  while(1)
  {
    ping_logger->wait_for_log_entry();
    ping_logger->process_log_entry();
  }
}

typedef struct 
{
  pingo_writer_arguments_s  args;
  ping_logger_c            *ping_logger;
  file_manager_c           *file_manager;
} writer_thread_args_s;

void *writer_thread_f(void* arg)
{
  writer_thread_args_s* writer_thread_args = (writer_thread_args_s*) arg;
  ping_logger_c *ping_logger;
  file_manager_c *file_manager;
  ping_block_c  *ping_block;
  unsigned int ping_block_counter = 0;
  struct timespec soak_time;
  struct timespec remaining_soak_time, time_since_dispatch, dispatch_time;
  char ip_string_buffer[IP_STRING_SIZE];
  ping_block_stats_s ping_block_stats;

  assert(writer_thread_args);
  ping_logger = writer_thread_args->ping_logger;
  assert(ping_logger);
  file_manager = writer_thread_args->file_manager;
  assert(file_manager);

  soak_time = 
    {
      .tv_sec = ((PINGO_ARGUMENT_VALID == writer_thread_args->args.soak_timeout_status)?writer_thread_args->args.soak_timeout:60), 
      .tv_nsec = 0
    };

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
      printf("Soaking for %lu.%03lu more seconds.\n", remaining_soak_time.tv_sec, NANOSEC_TO_MS(remaining_soak_time.tv_nsec));
      nanosleep(&remaining_soak_time, nullptr);
    }
    assert(ping_block == ping_logger->pop_ping_block());
    time_since_dispatch = ping_block->time_since_dispatch();
    ping_block_stats = ping_block->get_stats();
    printf("Soaked %lu.%03lu seconds.  %u/%u (%u%%) replied (min:%u, mean:%u, max:%u)\n", 
      time_since_dispatch.tv_sec, NANOSEC_TO_MS(time_since_dispatch.tv_nsec),
      ping_block_stats.valid_replies, ping_block->get_address_count(), (ping_block_stats.valid_replies*100)/ping_block->get_address_count(),
      ping_block_stats.min_reply_time, ping_block_stats.mean_reply_time, ping_block_stats.max_reply_time);
    printf("Writing ping block to file\n");
    file_manager->write_ping_block_to_file(ping_block);
    delete ping_block;
    printf("Deleted ping block.\n");
    ping_block_counter++;
  }

  return nullptr;
}

typedef struct 
{
  pingo_ping_block_arguments_s  ping_block_args;
  ping_logger_c                *ping_logger;
  uint32_t                      ping_block_first_address;
} send_thread_args_s;

void *send_thread_f(void* arg)
{
  ping_block_config_s    ping_block_config;
  ping_block_c          *ping_block;
  send_thread_args_s    *send_thread_args = (send_thread_args_s*) arg;
  ping_logger_c         *ping_logger;
  uint32_t               ping_block_first_address;
  unsigned int           ping_block_address_count = 65536;
  const struct timespec  cool_down = {.tv_sec = 0, .tv_nsec = 0};

  assert(send_thread_args);
  assert(send_thread_args->ping_logger);
  ping_logger = send_thread_args->ping_logger;

  ping_block_c::init_config(&ping_block_config); 
  ping_block_config.verbose = false;
  ping_block_config.fixed_sequence_number = true;
  ping_block_config.sequence_number = getpid();

  if(PINGO_ARGUMENT_VALID == send_thread_args->ping_block_args.initial_ip_status)
  {
    ping_block_first_address = send_thread_args->ping_block_args.initial_ip; 
  }
  else
  {
    ping_block_first_address = send_thread_args->ping_block_first_address;
  }
  if(PINGO_ARGUMENT_VALID == send_thread_args->ping_block_args.address_length_status)
  {
    ping_block_address_count = send_thread_args->ping_block_args.address_length; 
  }
  if(PINGO_ARGUMENT_VALID == send_thread_args->ping_block_args.cooldown_status)
  {
    MS_TO_TIMESPEC(send_thread_args->ping_block_args.cooldown, ping_block_config.ping_batch_cooldown); 
  }

  while(1)
  {
    ping_block = new ping_block_c(ping_block_first_address, ping_block_address_count, &ping_block_config);
    ping_block_first_address = ping_block->get_last_address();
    ping_logger->push_ping_block(ping_block);
    ping_block->dispatch();
    nanosleep(&cool_down, nullptr);
  }

  return nullptr;
}

void *recv_thread_f(void* arg)
{
  ping_logger_c         *ping_logger = (ping_logger_c*) arg;
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
  const uint16_t sequence_id = getpid();
  ping_log_entry_s log_entry;
  struct sockaddr_in src_addr;
  socklen_t addrlen;
  ipv4_word_t buffer[IPV4_MAX_PACKET_SIZE_WORDS];

  memset(&pingo_payload, 0, sizeof(pingo_payload));
  memset(&ping_reply_time, 0, sizeof(ping_reply_time));
  memset(&time_diff, 0, sizeof(time_diff));

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
    if(sizeof(struct sockaddr_in) != addrlen)
    {
      fprintf(stderr, "Received packet src_addr length unexpected.  addrlen %u expected %lu\n", addrlen, sizeof(struct sockaddr_in));
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
                    (sequence_id == icmp_packet_meta.header.rest_of_header.id_seq_num.sequence_number) &&
                    (ipv4_packet_meta.header.source_ip == pingo_payload.dest_address) &&
                    (timespec_valid(&pingo_payload.request_time)) )
                {

                  memset(&log_entry, 0, sizeof(log_entry));
                  log_entry.header.type=PING_LOG_ENTRY_ECHO_REPLY;
                  log_entry.data.echo_reply.payload = pingo_payload;
                  diff_timespec(&ping_reply_time, &pingo_payload.request_time, &log_entry.data.echo_reply.reply_delay);
                  ping_logger->push_log_entry(log_entry);

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
                  fprintf(stderr, "Invalid echo reply payload from %s.  identifier 0x%x (expected 0x%x) sequence %u (expected %u) pingo_dest_address %s pingo_request_time %lu.%09lus\n", 
                          ip_string_buffer_a, 
                          icmp_packet_meta.header.rest_of_header.id_seq_num.identifier, 
                          ICMP_IDENTIFIER,
                          icmp_packet_meta.header.rest_of_header.id_seq_num.sequence_number, 
                          sequence_id,
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


bool parse_pingo_args(int argc, char *argv[], pingo_arguments_s* args)
{
  bool ret_val = true;
  char o,c;

  if(args)
  {
    memset(args, 0, sizeof(pingo_arguments_s));

    while((o = getopt(argc, argv, "c:d:hi:s:t:v")) != ((char) -1))
    {
      switch(o)
      {
        case 'c':
        {
          if((sscanf(optarg, "%u%c", &args->ping_block_args.cooldown, &c) == 1))
          {
            args->ping_block_args.cooldown_status = PINGO_ARGUMENT_VALID;
          }
          else
          {
            args->ping_block_args.cooldown_status = PINGO_ARGUMENT_INVALID;
            fprintf(stderr, "-s %s: ping block cooldown format incorrect.  Expected ms as decimal integer.\n\n", optarg);
            args->unexpected_arg = true;
          }
          break;
        }
        case 'd':
        {
          args->writer_args.directory_status = PINGO_ARGUMENT_VALID;
          strncpy(args->writer_args.directory, optarg, sizeof(args->writer_args.directory));
          break;
        }
        case 'h':
        {
          args->help_request = PINGO_ARGUMENT_VALID;
          break;
        }
        case 'i':
        {
          uint8_t ip_a, ip_b, ip_c, ip_d;
          if(sscanf(optarg, "%hhu.%hhu.%hhu.%hhu%c", &ip_a, &ip_b, &ip_c, &ip_d, &c) == 4)
          {
            args->ping_block_args.initial_ip_status = PINGO_ARGUMENT_VALID;
            args->ping_block_args.initial_ip = (ip_a<<24) | (ip_b<<16) | (ip_c<<8) | (ip_d);
          }
          else
          {
            args->ping_block_args.initial_ip_status = PINGO_ARGUMENT_INVALID;
            fprintf(stderr, "-i %s: initial IP address format incorrect.  Expected IP in decimal format \'###.###.###.###\'.\n\n", optarg);
            args->unexpected_arg = true;
          }
          break;
        }
        case 's':
        {
          if((sscanf(optarg, "%u%c", &args->ping_block_args.address_length, &c) == 1))
          {
            args->ping_block_args.address_length_status = PINGO_ARGUMENT_VALID;
          }
          else
          {
            args->ping_block_args.address_length_status = PINGO_ARGUMENT_INVALID;
            fprintf(stderr, "-s %s: ping block size format incorrect.  Expected unsigned decimal integer.\n\n", optarg);
            args->unexpected_arg = true;
          }
          break;
        }
        case 't':
        {
          if((sscanf(optarg, "%u%c", &args->writer_args.soak_timeout, &c) == 1))
          {
            args->writer_args.soak_timeout_status = PINGO_ARGUMENT_VALID;
          }
          else
          {
            args->writer_args.soak_timeout_status = PINGO_ARGUMENT_INVALID;
            fprintf(stderr, "-t %s: soak timeout format incorrect.  Expected unsigned decimal integer.\n\n", optarg);
            args->unexpected_arg = true;
          }
          break;
        }
        case 'v':
        {
          args->validate_status = PINGO_ARGUMENT_VALID;
          break;
        }
        case '?':
        {
          args->unexpected_arg = true;
          break;
        }
        default:
        {
          ret_val = false;
          break;
        }
      }
    }
  }
  else
  {
    ret_val = false;
  }

  return ret_val;
}

int main(int argc, char *argv[])
{
  pingo_arguments_s args;
  ping_logger_c ping_logger;
  pthread_t log_handler_thread, writer_thread, recv_thread, send_thread;
  file_manager_c *file_manager;
  send_thread_args_s send_thread_args;
  writer_thread_args_s writer_thread_args;


  hilbert_curve_c hilbert_curve(16);
  printf("Hilbert max value %lu max coordinate %u\n", hilbert_curve.max_index(), hilbert_curve.max_coordinate());
  safe_exit(0);
  

  assert(parse_pingo_args(argc, argv, &args));

  if((args.unexpected_arg) || (PINGO_ARGUMENT_UNSPECIFIED != args.help_request))
  {
    int status = (PINGO_ARGUMENT_VALID == args.help_request?0:1);
    if(args.unexpected_arg)
    {
      status = 22;
    }

    printf("%s\n", help_string);
    exit(status);
  }

  file_manager = new file_manager_c((PINGO_ARGUMENT_VALID == args.writer_args.directory_status)?args.writer_args.directory:".");
  file_manager->build_registry();

  if(PINGO_ARGUMENT_VALID == args.validate_status)
  {
    printf("Validating Pingo files.\n");
    if(file_manager->validate_files_in_registry())
    {
      printf("Pingo files validated and complete!\n");
    }
    else
    {
      printf("Pingo files incomplete or corrupted!\n");
    }
  }
  else
  {
    memset(&send_thread_args, 0, sizeof(send_thread_args));
    send_thread_args.ping_block_first_address = file_manager->get_next_registry_hole_ip();
    send_thread_args.ping_block_args = args.ping_block_args;
    send_thread_args.ping_logger     = &ping_logger;

    memset(&writer_thread_args, 0, sizeof(writer_thread_args));
    writer_thread_args.args         = args.writer_args;
    writer_thread_args.ping_logger  = &ping_logger;
    writer_thread_args.file_manager = file_manager;

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGQUIT, signal_handler);

    pthread_create(&log_handler_thread, NULL, log_handler_thread_f, &ping_logger);
    pthread_create(&writer_thread,      NULL, writer_thread_f, &writer_thread_args);
    pthread_create(&recv_thread,        NULL, recv_thread_f,   &ping_logger);
    pthread_create(&send_thread,        NULL, send_thread_f,   &send_thread_args);

    while('q' != getchar()) {}
  }

  safe_exit(0);
}
