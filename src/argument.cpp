#include <cassert>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "argument.hpp"
#include "version.hpp"

using namespace sandor_laboratories::pingo;

static const char *help_string = PROJECT_NAME " " PROJECT_VER " <" PROJECT_URL ">\n"
                                 PROJECT_DESCRIPTION "\n\n"
                                 "Options:\n"
                                 "  -A: Annotate PNG with 256 Hilbert curve labels\n"
                                 "  -a: Author name to embed in PNG metadata\n"
                                 "  -c: Cooldown time in milliseconds between ping block batches\n"
                                 "  -D: Pixel depth used for creating PNG (1, 2, 4, or 8)\n"
                                 "        Intensity scaled to response time relative to 60 seconds or timeout given with -t\n"
                                 "  -d: Directory to read and write ping data\n"
                                 "  -e: File containing a list of CIDR address to Exclude from scan (one CIDR per line)\n"
                                 "  -i: Initial IP address to ping\n"
                                 "  -r: Reserve some number of color channels in PNG palette for user annotation\n"
                                 "        Required to leave a minimum two channels for plotting reply/no reply data ((2^depth)-reserved_channels >= 2)\n"
                                 "  -s: Size of ping blocks\n"
                                 "  -t: Ping block soaking Timeout\n"
                                 "  -v: Validate pingo files at directory and exit\n"
                                 "  -H: Create PNG of Hilbert Curve with given order starting at 0.0.0.0 or IP provided with -i\n"
                                 "  -h: Display this Help text\n";

const char * sandor_laboratories::pingo::get_help_string()
{
  return help_string;
}

inline void parse_cooldown_option(pingo_arguments_s* args)
{
  char dummy;
  assert(args != nullptr);

  if((sscanf(optarg, "%u%c", &args->ping_block_args.cooldown, &dummy) == 1))
  {
    args->ping_block_args.cooldown_status = PINGO_ARGUMENT_VALID;
  }
  else
  {
    args->ping_block_args.cooldown_status = PINGO_ARGUMENT_INVALID;
    fprintf(stderr, "-s %s: ping block cooldown format incorrect.  Expected ms as decimal integer.\n\n", optarg);
    args->unexpected_arg = true;
  }
}

inline bool parse_option(const int option, pingo_arguments_s* args)
{
  bool ret_val = true;

  assert(args != nullptr);

  switch(option)
  {
    case 'A':
    {
      args->image_args.annotate_status = PINGO_ARGUMENT_VALID;
      break;
    }
    case 'a':
    {
      args->image_args.hilbert_image_author_status = PINGO_ARGUMENT_VALID;
      strncpy(args->image_args.hilbert_image_author, optarg, sizeof(args->image_args.hilbert_image_author));
      break;
    }
    case 'c':
    {
      parse_cooldown_option(args);
      break;
    }
    case 'D':
    {
      char dummy;
      if( (sscanf(optarg, "%u%c", &args->image_args.pixel_depth, &dummy) == 1) &&
          ( (args->image_args.pixel_depth == BITS_1) ||
            (args->image_args.pixel_depth == BITS_2) ||
            (args->image_args.pixel_depth == BITS_4) ||
            (args->image_args.pixel_depth == BITS_8) )  )
      {
        args->image_args.pixel_depth_status = PINGO_ARGUMENT_VALID;
      }
      else
      {
        args->image_args.pixel_depth_status = PINGO_ARGUMENT_INVALID;
        fprintf(stderr, "-D %s: Pixel depth format incorrect.  Expected decimal integer 1,2,4, or 8.\n\n", optarg);
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
    case 'e':
    {
      args->ping_block_args.exclude_list_status = PINGO_ARGUMENT_VALID;
      strncpy(args->ping_block_args.exclude_list_path, optarg, sizeof(args->ping_block_args.exclude_list_path));
      break;
    }
    case 'H':
    {
      char dummy;
      if( (sscanf(optarg, "%u%c", &args->image_args.hilbert_image_order, &dummy) == 1) &&
          (args->image_args.hilbert_image_order >   0) &&
          (args->image_args.hilbert_image_order <= HILBERT_ORDER_FOR_32_BITS) )
      {
        args->image_args.hilbert_image_order_status = PINGO_ARGUMENT_VALID;
      }
      else
      {
        args->image_args.hilbert_image_order_status = PINGO_ARGUMENT_INVALID;
        fprintf(stderr, "-H %s: Hilbert Curve order format incorrect.  Expected order as unsigned decimal integer <= 16.\n\n", optarg);
        args->unexpected_arg = true;
      }

      break;
    }
    case 'h':
    {
      args->help_request = PINGO_ARGUMENT_VALID;
      break;
    }
    case 'i':
    {
      uint8_t ip_a;
      uint8_t ip_b;
      uint8_t ip_c;
      uint8_t ip_d;

      char dummy;
      if(sscanf(optarg, "%hhu.%hhu.%hhu.%hhu%c", &ip_a, &ip_b, &ip_c, &ip_d, &dummy) == 4)
      {
        args->ping_block_args.initial_ip_status = PINGO_ARGUMENT_VALID;
        args->ping_block_args.initial_ip = (ip_a<<IP_BYTE_A_OFFSET) | (ip_b<<IP_BYTE_B_OFFSET) | (ip_c<<IP_BYTE_C_OFFSET) | (ip_d);
      }
      else
      {
        args->ping_block_args.initial_ip_status = PINGO_ARGUMENT_INVALID;
        fprintf(stderr, "-i %s: initial IP address format incorrect.  Expected IP in decimal format \'###.###.###.###\'.\n\n", optarg);
        args->unexpected_arg = true;
      }
      break;
    }
    case 'r':
    {
      char dummy;
      if((sscanf(optarg, "%u%c", &args->image_args.reserved_colors, &dummy) == 1))
      {
        args->image_args.reserved_color_status = PINGO_ARGUMENT_VALID;
      }
      else
      {
        args->image_args.reserved_color_status = PINGO_ARGUMENT_INVALID;
        fprintf(stderr, "-r %s: Reserved color format incorrect.  Expected unsigned decimal integer.\n\n", optarg);
        args->unexpected_arg = true;
      }
      break;
    }
      case 's':
    {
      char dummy;
      if((sscanf(optarg, "%u%c", &args->ping_block_args.address_length, &dummy) == 1))
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
      char dummy;
      if((sscanf(optarg, "%u%c", &args->writer_args.soak_timeout, &dummy) == 1))
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

  return ret_val;
}

bool sandor_laboratories::pingo::parse_pingo_args(int argc, char *argv[], pingo_arguments_s* args)
{
  bool ret_val = true;
  int option;

  if(args != nullptr)
  {
    memset(args, 0, sizeof(pingo_arguments_s));

    while((option = getopt(argc, argv, "Aa:c:D:d:e:H:hi:r:s:t:v")) !=  -1)
    {
      if(!parse_option(option, args))
      {
        ret_val = false;
      }
    }
  }
  else
  {
    ret_val = false;
  }

  return ret_val;
}