#ifndef __ARGUMENT_HPP__
#define __ARGUMENT_HPP__

#include <cstdint>
#include <ctime>

#include "pingo.hpp"

namespace sandor_laboratories
{
  namespace pingo
  {
    /* Argument Types */
    typedef enum
    {
      PINGO_ARGUMENT_UNSPECIFIED,
      PINGO_ARGUMENT_VALID,
      PINGO_ARGUMENT_INVALID,
    } pingo_argument_status_e;

    #define IMAGE_STRING_BUFFER_SIZE 1024
    typedef struct
    {
      pingo_argument_status_e pixel_depth_status;
      uint32_t                pixel_depth;
      pingo_argument_status_e reserved_color_status;
      uint32_t                reserved_colors;
      pingo_argument_status_e hilbert_image_order_status;
      unsigned int            hilbert_image_order;
      pingo_argument_status_e hilbert_image_author_status;
      char                    hilbert_image_author[IMAGE_STRING_BUFFER_SIZE];

    } pingo_image_arguments_s;

    typedef struct
    {
      pingo_argument_status_e initial_ip_status;
      uint32_t                initial_ip;

      pingo_argument_status_e address_length_status;
      uint32_t                address_length;

      pingo_argument_status_e cooldown_status;
      uint32_t                cooldown;

      pingo_argument_status_e exclude_list_status;
      char                    exclude_list_path[FILE_PATH_MAX_LENGTH];

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

      pingo_argument_status_e      threads_status;
      uint32_t                     threads;

      pingo_image_arguments_s      image_args;
      pingo_ping_block_arguments_s ping_block_args;
      pingo_writer_arguments_s     writer_args;

    } pingo_arguments_s;

    bool parse_pingo_args(int argc, char *argv[], pingo_arguments_s* args);

    const char * get_help_string();
  }
}

#endif /* __ARGUMENT_HPP__ */