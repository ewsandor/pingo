#include <cstdio>
#include <cstring>

#include "graphic.hpp"

using namespace sandor_laboratories::pingo::graphic;

grayscale_t rbg_to_grayscale(const rgb_s* rgb)
{
  const unsigned int channel_total = (((unsigned int) rgb->red  ) + 
                                      ((unsigned int) rgb->green) + 
                                      ((unsigned int) rgb->blue ) );
  
  return ((channel_total / 3) & 0xFF);
}

bool get_rgb_at_coordinate(const graphic_s *graphic, const coordinate_s *coordinate, rgb_s *output_rgb)
{
  bool ret_val = true;

  if(graphic && coordinate && output_rgb)
  {
    if((coordinate->x < graphic->width) && (coordinate->y < graphic->height))
    {
      memset(output_rgb, 0, sizeof(rgb_s));
      unsigned int pixel_index = (coordinate->y*graphic->width*GRAPHIC_RGB_SIZE_BYTES)+(coordinate->x*GRAPHIC_RGB_SIZE_BYTES);
      output_rgb->red   = graphic->data[pixel_index];
      output_rgb->green = graphic->data[pixel_index+1];
      output_rgb->blue  = graphic->data[pixel_index+2];
    }
    else
    {
      fprintf(stderr, "Coordinate out of bound of graphic.  x %u y %u width %u height %u\n", 
        coordinate->x, coordinate->y, graphic->width, graphic->height);
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null input to get_rgb_at_coordinate.  graphic %p coordinate %p output_rgb %p\n", 
      graphic, coordinate, output_rgb);
    ret_val = false;
  }

  return ret_val;
}

extern "C"
{
  extern const struct { unsigned int width; unsigned int height; unsigned int bytes_per_pixel; unsigned char	 pixel_data[614 * 1396 * 3 + 1]; } graphic_digit_0;
  extern const struct { unsigned int width; unsigned int height; unsigned int bytes_per_pixel; unsigned char	 pixel_data[614 * 1396 * 3 + 1]; } graphic_digit_1;
  extern const struct { unsigned int width; unsigned int height; unsigned int bytes_per_pixel; unsigned char	 pixel_data[614 * 1396 * 3 + 1]; } graphic_digit_2;
  extern const struct { unsigned int width; unsigned int height; unsigned int bytes_per_pixel; unsigned char	 pixel_data[614 * 1396 * 3 + 1]; } graphic_digit_3;
  extern const struct { unsigned int width; unsigned int height; unsigned int bytes_per_pixel; unsigned char	 pixel_data[614 * 1396 * 3 + 1]; } graphic_digit_4;
  extern const struct { unsigned int width; unsigned int height; unsigned int bytes_per_pixel; unsigned char	 pixel_data[614 * 1396 * 3 + 1]; } graphic_digit_5;
  extern const struct { unsigned int width; unsigned int height; unsigned int bytes_per_pixel; unsigned char	 pixel_data[614 * 1396 * 3 + 1]; } graphic_digit_6;
  extern const struct { unsigned int width; unsigned int height; unsigned int bytes_per_pixel; unsigned char	 pixel_data[614 * 1396 * 3 + 1]; } graphic_digit_7;
  extern const struct { unsigned int width; unsigned int height; unsigned int bytes_per_pixel; unsigned char	 pixel_data[614 * 1396 * 3 + 1]; } graphic_digit_8;
  extern const struct { unsigned int width; unsigned int height; unsigned int bytes_per_pixel; unsigned char	 pixel_data[614 * 1396 * 3 + 1]; } graphic_digit_9;
}

graphic_s graphic_digit[] = 
{
  {.width = graphic_digit_0.width, .height = graphic_digit_0.height, .data = graphic_digit_0.pixel_data},
  {.width = graphic_digit_1.width, .height = graphic_digit_1.height, .data = graphic_digit_1.pixel_data},
  {.width = graphic_digit_2.width, .height = graphic_digit_2.height, .data = graphic_digit_2.pixel_data},
  {.width = graphic_digit_3.width, .height = graphic_digit_3.height, .data = graphic_digit_3.pixel_data},
  {.width = graphic_digit_4.width, .height = graphic_digit_4.height, .data = graphic_digit_4.pixel_data},
  {.width = graphic_digit_5.width, .height = graphic_digit_5.height, .data = graphic_digit_5.pixel_data},
  {.width = graphic_digit_6.width, .height = graphic_digit_6.height, .data = graphic_digit_6.pixel_data},
  {.width = graphic_digit_7.width, .height = graphic_digit_7.height, .data = graphic_digit_7.pixel_data},
  {.width = graphic_digit_8.width, .height = graphic_digit_8.height, .data = graphic_digit_8.pixel_data},
  {.width = graphic_digit_9.width, .height = graphic_digit_9.height, .data = graphic_digit_9.pixel_data},
};

const graphic_s *get_graphic_for_digit(uint_fast8_t digit)
{
  const graphic_s * ret_ptr = nullptr;

  if(digit < 10)
  {
    ret_ptr = &graphic_digit[digit];
  }
  else
  {
   fprintf(stderr, "Digit out of range, returning nullptr.  digit %u\n", digit);
  }

  return ret_ptr;
}