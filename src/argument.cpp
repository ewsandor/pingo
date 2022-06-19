#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "argument.hpp"
#include "version.hpp"

using namespace sandor_laboratories::pingo;

static const char *help_string = PROJECT_NAME " " PROJECT_VER " <" PROJECT_URL ">\n"
                                 PROJECT_DESCRIPTION "\n\n"
                                 "Options:\n"
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

bool sandor_laboratories::pingo::parse_pingo_args(int argc, char *argv[], pingo_arguments_s* args)
{
  bool ret_val = true;
  char o,c;

  if(args)
  {
    memset(args, 0, sizeof(pingo_arguments_s));

    while((o = getopt(argc, argv, "a:c:D:d:e:H:hi:r:s:t:v")) != ((char) -1))
    {
      switch(o)
      {
        case 'a':
        {
          args->image_args.hilbert_image_author_status = PINGO_ARGUMENT_VALID;
          strncpy(args->image_args.hilbert_image_author, optarg, sizeof(args->image_args.hilbert_image_author));
          break;
        }
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
        case 'D':
        {
          if( (sscanf(optarg, "%u%c", &args->image_args.pixel_depth, &c) == 1) &&
              ( (args->image_args.pixel_depth == 1) ||
                (args->image_args.pixel_depth == 2) ||
                (args->image_args.pixel_depth == 4) ||
                (args->image_args.pixel_depth == 8) )  )
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
          if( (sscanf(optarg, "%u%c", &args->image_args.hilbert_image_order, &c) == 1) &&
              (args->image_args.hilbert_image_order >   0) &&
              (args->image_args.hilbert_image_order <= 16) )
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
        case 'r':
        {
          if((sscanf(optarg, "%u%c", &args->image_args.reserved_colors, &c) == 1))
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