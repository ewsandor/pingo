#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "file.hpp"
#include "icmp.hpp"
#include "ipv4.hpp"
#include "ping_block.hpp"
#include "ping_logger.hpp"
#include "pingo.hpp"

#include "hilbert.hpp"
#include "image.hpp"

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
  if(mutex_code != 0)
  {
    fprintf(stderr, "UNSAFE EXIT! Failed to lock exit mutex.  mutex_code %d %s\n", mutex_code, strerror(mutex_code));
    exit(1);
  }
  else
  {
    while(exit_block_mask != 0)
    {
      fprintf(((0 == status)?stdout:stderr), "Waiting for safe exit.  Exit block mask 0x%x\n", exit_block_mask);
      cond_code = pthread_cond_timedwait(&exit_cond, &exit_mutex, &exit_timeout_abs);
      if(cond_code != 0)
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

bool sandor_laboratories::pingo::timespec_valid(const struct timespec * test_timespec)
{
  bool ret_val = false;

  if(test_timespec != nullptr)
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
  
  if(diff != nullptr)
  {
    memset(diff, 0, sizeof(*diff));

    if((a != nullptr) && (b != nullptr))
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
  if(buffer != nullptr)
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

  while(true)
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

  while(true)
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
    printf("Soaked %lu.%03lu seconds.  %u/%u (%u%%) replied (min:%u, mean:%u, max:%u skipped: %u)\n", 
      time_since_dispatch.tv_sec, NANOSEC_TO_MS(time_since_dispatch.tv_nsec),
      ping_block_stats.valid_replies, ping_block->get_address_count(), (ping_block_stats.valid_replies*100)/ping_block->get_address_count(),
      ping_block_stats.min_reply_time, ping_block_stats.mean_reply_time, ping_block_stats.max_reply_time, ping_block_stats.skipped_pings);
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
  ping_block_excluded_ip_list_t *excluded_ip_list;
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
  ping_block_config.excluded_ip_list = send_thread_args->excluded_ip_list;

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

  while(true)
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

  while(true)
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
              if(icmp_packet_meta.payload_size == sizeof(pingo_payload_t))
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
                        (unsigned int)icmp_packet_meta.header_valid,
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
            ip_string_buffer_a, (unsigned int) icmp_packet_meta.header_valid);
        }
      }
      else
      {
        ip_string(ntohl(src_addr.sin_addr.s_addr), ip_string_buffer_a, sizeof(ip_string_buffer_a));
        fprintf(stderr, "Invalid packet from %s.  IPv4 header valid %u\n", 
          ip_string_buffer_a, (unsigned int) ipv4_packet_meta.header_valid);
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

inline uint32_t cidr_subnet_to_subnet_mask(unsigned int subnet)
{
  subnet = ((subnet<=32)?subnet:32);
  return (((1L << subnet)-1) << (32-subnet));
}

bool load_ping_block_exclude_list(char * path, ping_block_excluded_ip_list_t * exclude_list)
{
  bool ret_val = true;

  printf("Reading ping block IP exclude list '%s'.\n", path);

  if((path != nullptr) && (exclude_list != nullptr))
  {
    FILE * fp = fopen(path, "r");

    if(fp != nullptr)
    {
      unsigned int line_number = 1;
      while(feof(fp) == 0)
      {
        char line[1024];
        uint8_t byte_a, byte_b, byte_c, byte_d, subnet;

        if((line == fgets(line, 1024, fp)) &&
           (line[0] != '#') &&
           (line[0] != '\n'))
        {
          const unsigned int items_read = sscanf(line, "%hhu.%hhu.%hhu.%hhu/%hhu", &byte_a, &byte_b, &byte_c, &byte_d, &subnet);
          if ((4 == items_read) ||
              (5 == items_read))
          {
            const ping_block_excluded_ip_s exclude_ip =
              {
                .ip = (uint32_t)((byte_a << 24) | (byte_b << 16) | (byte_c << 8) | (byte_d)),
                .subnet_mask = cidr_subnet_to_subnet_mask((5 == items_read)?subnet:32),
              };
            exclude_list->push_back(exclude_ip);

            char ip_string_buffer[IP_STRING_SIZE];
            char subnet_string_buffer[IP_STRING_SIZE];
            ip_string(exclude_ip.ip, ip_string_buffer, sizeof(ip_string_buffer));
            ip_string(exclude_ip.subnet_mask, subnet_string_buffer, sizeof(subnet_string_buffer));
            printf("Loaded IP %s with subnet mask %s from exclude list file.\n", ip_string_buffer, subnet_string_buffer);
          }
          else
          {
            fprintf(stderr, "Line %u: Unexpected IP format.  Expected CIDR format ###.###.###.###/##. - %s", line_number, line);
          }
        }

        line_number++;
      }
      assert(0 == fclose(fp));
    }
    else
    {
      fprintf(stderr, "Failed to open exclude list file '%s'.  errno %u: %s\n", path, errno, strerror(errno));
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null inputs to load exclude list.  path %p excluded_list %p", path, exclude_list);
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

  signal(SIGINT,  signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGQUIT, signal_handler);

//  #define HILBERT_TEST
  #ifdef HILBERT_TEST
  hilbert_curve_c hilbert_curve(8);
  printf("Hilbert max value %lu max coordinate %u\n", hilbert_curve.max_index(), hilbert_curve.max_coordinate());
  const hilbert_index_t max_index = hilbert_curve.max_index();
  for(hilbert_index_t i = 0; i < max_index; i++)
  {
    hilbert_coordinate_s coordinate;
    assert(hilbert_curve.get_coordinate(i, &coordinate));
//    printf("{ .x = %2u, .y = %2u },\n", coordinate.x, coordinate.y);
//    printf("%u, %u\n", coordinate.x, coordinate.y);
//    printf("%lu - %u, %u\n", i, coordinate.x, coordinate.y);
  }
  safe_exit(0);
  #endif

  assert(parse_pingo_args(argc, argv, &args));

  if((args.unexpected_arg) || (PINGO_ARGUMENT_UNSPECIFIED != args.help_request))
  {
    int status = (PINGO_ARGUMENT_VALID == args.help_request?0:1);
    if(args.unexpected_arg)
    {
      status = 22;
    }

    printf("%s\n", get_help_string());
    exit(status);
  }

  file_manager = new file_manager_c((PINGO_ARGUMENT_VALID == args.writer_args.directory_status)?args.writer_args.directory:".");

  if(PINGO_ARGUMENT_VALID == args.validate_status)
  {
    printf("Validating Pingo files.\n");

    file_manager->build_registry();

    if(file_manager->validate_files_in_registry())
    {
      printf("Pingo files validated and complete!\n");
    }
    else
    {
      printf("Pingo files incomplete or corrupted!\n");
    }
  }
  else if(PINGO_ARGUMENT_VALID == args.image_args.hilbert_image_order_status)
  {
    png_config_s png_config;
    init_png_config(&png_config);

    png_config.image_args   = args.image_args;
    png_config.file_manager = file_manager;
    if(PINGO_ARGUMENT_VALID == args.ping_block_args.initial_ip_status)
    {
      png_config.initial_ip = args.ping_block_args.initial_ip;
    }
    if(PINGO_ARGUMENT_VALID == args.image_args.reserved_color_status)
    {
      png_config.reserved_colors = args.image_args.reserved_colors;
    }
    png_config.color_depth = ((PINGO_ARGUMENT_VALID == args.image_args.pixel_depth_status)?args.image_args.pixel_depth:1);
    png_config.depth_scale_reference = SECONDS_TO_MS(((PINGO_ARGUMENT_VALID == args.writer_args.soak_timeout_status)?args.writer_args.soak_timeout:60));

    if( (png_config.reserved_colors > (1U << png_config.color_depth)) ||
        (((1 << png_config.color_depth)-png_config.reserved_colors) < 2) )
    {
      fprintf(stderr, 
        "Too many reserved colors requested for generating PNG.  Pixel depth %u (%u colors) reserved %u colors.  2 colors needed to plot ping replies.\n",
        png_config.color_depth, (1U << png_config.color_depth), png_config.reserved_colors);
      safe_exit(1);
    }

    char ip_string_buffer[IP_STRING_SIZE];
    ip_string(png_config.initial_ip, ip_string_buffer, sizeof(ip_string_buffer), '_', true);
    snprintf(png_config.image_file_path, sizeof(png_config.image_file_path), 
            "%s_hilbert_%02u_color_depth_%u_timeout_%03u_reserved_%03u.png", 
            ip_string_buffer, png_config.image_args.hilbert_image_order, png_config.color_depth,
            MS_TO_SECONDS(png_config.depth_scale_reference), png_config.reserved_colors);

    printf("Scanning data files. \n"); 
    file_manager->build_registry();
    generate_png_image(&png_config);
  }
  else
  {
    memset(&send_thread_args, 0, sizeof(send_thread_args));
    send_thread_args.ping_block_first_address = 0;
    send_thread_args.ping_block_args = args.ping_block_args;
    send_thread_args.ping_logger     = &ping_logger;
    if(PINGO_ARGUMENT_VALID == args.ping_block_args.exclude_list_status)
    {
      printf("Reading excluded IP list.\n");
      send_thread_args.excluded_ip_list = new ping_block_excluded_ip_list_t();
      if(!load_ping_block_exclude_list(args.ping_block_args.exclude_list_path, send_thread_args.excluded_ip_list))
      {
        fprintf(stderr, "Failed to load exclude list from %s.\n", args.ping_block_args.exclude_list_path);
        safe_exit(1);
      }
    }

    if(PINGO_ARGUMENT_VALID == args.ping_block_args.initial_ip_status)
    {
      send_thread_args.ping_block_first_address = args.ping_block_args.initial_ip; 
    }
    else
    {
      printf("Reading data directory to find first hole of ping data.\n");
      file_manager->build_registry();
      send_thread_args.ping_block_first_address = file_manager->get_next_registry_hole_ip();
    }

    memset(&writer_thread_args, 0, sizeof(writer_thread_args));
    writer_thread_args.args         = args.writer_args;
    writer_thread_args.ping_logger  = &ping_logger;
    writer_thread_args.file_manager = file_manager;

    pthread_create(&log_handler_thread, nullptr, log_handler_thread_f, &ping_logger);
    pthread_create(&writer_thread,      nullptr, writer_thread_f, &writer_thread_args);
    pthread_create(&recv_thread,        nullptr, recv_thread_f,   &ping_logger);
    pthread_create(&send_thread,        nullptr, send_thread_f,   &send_thread_args);

    while('q' != getchar()) {}
  }

  safe_exit(0);
}
