#ifndef __GRAPHIC_HPP__
#define __GRAPHIC_HPP__

#include <cstdint>

namespace sandor_laboratories
{
  namespace pingo
  {
    namespace graphic
    {
      typedef unsigned int coordinate_t;
      typedef struct 
      {
        coordinate_t x;
        coordinate_t y;
      } coordinate_s;

      typedef uint8_t color_channel_t;
      typedef struct
      {
        color_channel_t red;
        color_channel_t green;
        color_channel_t blue;
      } rgb_s;
      #define GRAPHIC_RGB_SIZE_BYTES 3

      typedef uint8_t grayscale_t;

      typedef struct
      {
        coordinate_t   width;
        coordinate_t   height;
        const uint8_t *data;
      } graphic_s;
 
      grayscale_t rbg_to_grayscale(const rgb_s* rgb);
      inline grayscale_t rbg_to_grayscale(rgb_s rgb) {return rbg_to_grayscale(&rgb);};

      bool get_rgb_at_coordinate(const graphic_s *graphic, const coordinate_s *coordinate, rgb_s *output_rgb);

      const graphic_s *get_graphic_for_digit(uint_fast8_t digit);
    }
  }
}




#endif /* __GRAPHIC_HPP__ */