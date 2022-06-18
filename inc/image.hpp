#ifndef __IMAGE_HPP__
#define __IMAGE_HPP__

#include <stdint.h>

#include "argument.hpp"
#include "file.hpp"
#include "pingo.hpp"

namespace sandor_laboratories
{
  namespace pingo
  {
    typedef struct
    {
      pingo_image_arguments_s  image_args;

      file_manager_c          *file_manager;
      char                     image_file_path[FILE_PATH_MAX_LENGTH];
      uint32_t                 initial_ip;
      unsigned int             color_depth;
      unsigned int             depth_scale_reference;

    } png_config_s;

    void init_png_config(png_config_s*);
    void generate_png_image(const png_config_s*);
  }
}

#endif /* __IMAGE_HPP__ */ 