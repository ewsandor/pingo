#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <png.h>
#include <stack>

#include "file.hpp"
#include "graphic.hpp"
#include "hilbert.hpp"
#include "image.hpp"
#include "pingo.hpp"
#include "version.hpp"

using namespace sandor_laboratories::pingo;

typedef struct
{
  const hilbert_curve_c *hilbert_curve;
  const png_config_s    *png_config;
  png_bytepp             row_pointers;
  
} hilbert_image_from_file_params_s;

inline void set_image_pixel(png_bytepp row_pointers, unsigned int color_depth, unsigned int width, unsigned int x, unsigned int y, unsigned int value)
{
  const unsigned int  pixels_per_byte  = (8/color_depth);
  const png_bytep     row_pointer      =  row_pointers[y];
  const unsigned int  pixel_depth_mask = ((1<<color_depth)-1);
  png_byte           *column_pointer   = &row_pointer[(x%(width))/pixels_per_byte];

  *column_pointer |= ((value & pixel_depth_mask) << ((x%pixels_per_byte)*color_depth));
}

inline void draw_digit_on_image(const hilbert_image_from_file_params_s * image_params, uint_fast8_t digit, unsigned int x_offset, unsigned int y_offset, unsigned int scale)
{
  assert(scale > 0);
  assert(digit < 10);
  const graphic::graphic_s * digit_graphic = graphic::get_graphic_for_digit(digit);
  unsigned int i,j;

  for(i = 0; i < digit_graphic->width/scale; i++)
  {
    for(j = 0; j < digit_graphic->height/scale; j++)
    {
      graphic::rgb_s rgb;
      graphic::get_rgb_at_coordinate(digit_graphic, i*scale, j*scale, &rgb);
      graphic::grayscale_t grayscale_value = graphic::rbg_to_grayscale(rgb);

      if(grayscale_value > 0)
      {
        set_image_pixel(image_params->row_pointers, 
                        image_params->png_config->color_depth, 
                        image_params->hilbert_curve->max_coordinate(), x_offset+i, y_offset+j,
                        ((1 << image_params->png_config->color_depth) - 1));
      }
    }
  }
}

inline void draw_number_on_image(const hilbert_image_from_file_params_s * image_params, unsigned int number, unsigned int x_offset, unsigned int y_offset, unsigned int scale = 1, uint_fast8_t min_digits = 1)
{
  std::stack<uint_fast8_t> digit_stack;

  unsigned int number_temp = number;
  while(number_temp)
  {
    digit_stack.push(number_temp % 10);
    number_temp /= 10;
  }

  while(digit_stack.size() < min_digits)
  {
    digit_stack.push(0);
  }
 
  while(!digit_stack.empty())
  {
    uint_fast8_t digit = digit_stack.top();
    draw_digit_on_image(image_params, digit, x_offset, y_offset, scale);
    const graphic::graphic_s * digit_graphic = graphic::get_graphic_for_digit(digit);
    x_offset += (digit_graphic->width/scale);
    digit_stack.pop();
  }
}

inline void annotate_hilbert_image(const hilbert_image_from_file_params_s * image_params)
{
  unsigned int i = 0;
  hilbert_curve_c hilbert_curve_4(4);
  hilbert_curve_c hilbert_curve_16(16);
  for(i = 0; i < hilbert_curve_4.max_index(); i++)
  {
    hilbert_coordinate_s coordinate;
    hilbert_curve_4.get_coordinate(i, &coordinate);
    draw_number_on_image( image_params, i, 
                          (coordinate.x*image_params->hilbert_curve->max_coordinate())/16, 
                          (coordinate.y*image_params->hilbert_curve->max_coordinate())/16,
                          hilbert_curve_16.max_coordinate()/image_params->hilbert_curve->max_coordinate());
  }
}

void fill_hilbert_image_from_file(const file_s* file, const void * user_data_ptr)
{
  assert(file);
  assert(user_data_ptr);
  char ip_string_buffer[IP_STRING_SIZE];

  const hilbert_image_from_file_params_s * params = (const hilbert_image_from_file_params_s *) user_data_ptr;

  ip_string(file->header.first_address, ip_string_buffer, sizeof(ip_string_buffer));
  printf("Filling image data for file starting at IP %s with %u IPs.\n", ip_string_buffer, file->header.address_count);

  const hilbert_index_t      max_index        = params->hilbert_curve->max_index();
  const hilbert_coordinate_t max_coordinate   = params->hilbert_curve->max_coordinate();
  const uint_fast64_t        last_ip          = ((uint_fast64_t)params->png_config->initial_ip) + max_index;
  const uint_fast64_t        file_last_ip     = file->header.first_address + file->header.address_count;
  const unsigned int         pixel_depth_mask = ((1<<params->png_config->color_depth)-1);
  const unsigned int         max_value        = (pixel_depth_mask - params->png_config->reserved_colors);

  for(uint_fast64_t i = MAX(params->png_config->initial_ip, file->header.first_address); i < MIN(last_ip, file_last_ip); i++)
  {
    const file_data_entry_s * file_data_entry = &file->data[(i-file->header.first_address)]; 
    if(FILE_DATA_ENTRY_ECHO_REPLY == file_data_entry->type)
    {
      const hilbert_index_t hilbert_index = (i-params->png_config->initial_ip);
      hilbert_coordinate_s  coordinate;
      assert(params->hilbert_curve->get_coordinate(hilbert_index, &coordinate));

      unsigned int value = 1;
      if(file_data_entry->payload.echo_reply.reply_time < params->png_config->depth_scale_reference)
      {
        value = max_value - ((file_data_entry->payload.echo_reply.reply_time * max_value)/params->png_config->depth_scale_reference);
      }
      set_image_pixel(params->row_pointers, params->png_config->color_depth, max_coordinate, coordinate.x, coordinate.y, value);
    }
  }
}

FILE * file_ptr = nullptr;
png_structp png_ptr = nullptr;
png_infop png_info_ptr = nullptr;
hilbert_coordinate_t max_coordinate = 0;
png_bytep row_pointers[MAX_IMAGE_DIMENSION_SIZE] = {0};

void sandor_laboratories::pingo::init_png_config(png_config_s* png_config)
{
  if(png_config != nullptr)
  {
    memset(png_config, 0, sizeof(png_config_s));
    png_config->color_depth = 1;
  }
}

void cleanup_png_data()
{
  if(file_ptr != nullptr)
  {
    printf("Closing file.\n");
    fclose(file_ptr);
  }
  printf("Destroying PNG data.\n");
  png_destroy_write_struct(&png_ptr, &png_info_ptr);

  printf("Freeing image memory.\n");
  for(unsigned int i = 0; i < max_coordinate; i++)
  {
    if(row_pointers[i] != nullptr)
    {
      free(row_pointers[i]);
    }
  }
  max_coordinate = 0;
}

void png_error_handler(png_structp png_struct_ptr, png_const_charp error_string)
{
  UNUSED(png_struct_ptr);

  fprintf(stderr, "libpng error - %s.\n", error_string);
  cleanup_png_data();
  safe_exit(1);
}

#define PNG_WRITE_PROGRESS_INTERVAL (4*256)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void png_write_row_cb(png_structp png_ptr, png_uint_32 row, int pass)
{
  UNUSED(png_ptr);
  UNUSED(pass);

  assert(max_coordinate);

  if(0 == row % PNG_WRITE_PROGRESS_INTERVAL)
  {
    printf("%3u%% of PNG written to file.\n", ((row*PERCENT_100)/max_coordinate));
  }
}

void allocate_image_memory(const png_config_s* png_config)
{
  assert(png_config != nullptr);

  const unsigned int pixels_per_byte = (8/png_config->color_depth);
  printf("Allocating image memory for %u x %u PNG image.  %u pixels_per_byte\n", max_coordinate, max_coordinate, pixels_per_byte);
  for(unsigned int i = 0; i < max_coordinate; i++)
  {
    row_pointers[i] = (png_byte*) calloc(sizeof(png_byte), max_coordinate/pixels_per_byte);
  }
}

void fill_png_palette(const png_config_s* png_config)
{
  assert(png_config != nullptr);

  printf("Filling PNG palette.\n");
  png_color palette[PNG_MAX_PALETTE_LENGTH];
  memset(palette, 0, sizeof(palette));
  assert((1 << png_config->color_depth) <= PNG_MAX_PALETTE_LENGTH);
  for(unsigned int i = 1; i < (1U << png_config->color_depth); i++)
  {
    const unsigned int max_value = ((1U << png_config->color_depth)-png_config->reserved_colors);
    unsigned int intensity = ((COLOR_8_BIT_MAX*i)/(max_value-1));

    if(i < max_value)
    {
      palette[i].red   = intensity;
      palette[i].green = intensity;
      palette[i].blue  = intensity;
    }
    else
    {
      palette[i].red   = (2 == (i%3))?COLOR_8_BIT_MAX:0;
      palette[i].green = (0 == (i%3))?COLOR_8_BIT_MAX:0;
      palette[i].blue  = (1 == (i%3))?COLOR_8_BIT_MAX:0;
    }
  }
  png_set_PLTE(png_ptr,png_info_ptr, palette, (1 << png_config->color_depth));
}

#define PNG_TEXT_BUFFER_SIZE 1024
#define YEAR_1900 1900
void fill_png_text(const png_config_s* png_config, const struct tm gmtime_now, hilbert_curve_c *hilbert_curve)
{
  assert(png_config != nullptr);
  assert(hilbert_curve != nullptr);
  printf("Filling PNG text info.\n");
  
  std::vector<png_text> png_text_array;
  char title_key[]      = "Title";
  char title_text[PNG_TEXT_BUFFER_SIZE];
  char ip_string_buffer_a[IP_STRING_SIZE];
  char ip_string_buffer_b[IP_STRING_SIZE];
  ip_string(png_config->initial_ip, ip_string_buffer_a, sizeof(ip_string_buffer_a));
  ip_string((((uint_fast64_t) png_config->initial_ip)+hilbert_curve->max_index())-1, ip_string_buffer_b, sizeof(ip_string_buffer_a));
  snprintf(title_text, sizeof(title_text), "ICMP Echo Replies (%s - %s)", ip_string_buffer_a, ip_string_buffer_b);
  png_text_array.push_back
    (
      {
        .compression = PNG_TEXT_COMPRESSION_NONE,
        .key = title_key,
        .text = title_text,
        .text_length = strlen(title_text),
        .itxt_length = 0,
        .lang = nullptr,
        .lang_key = nullptr,
      }
    );
  char description_key[]      = "Description";
  char description_text[PNG_TEXT_BUFFER_SIZE];
  ip_string(png_config->initial_ip, ip_string_buffer_a, sizeof(ip_string_buffer_a));
  ip_string((((uint_fast64_t) png_config->initial_ip)+hilbert_curve->max_index())-1, ip_string_buffer_b, sizeof(ip_string_buffer_a));
  snprintf(description_text, sizeof(description_text), "ICMP echo replies for IP addresses %s - %s plotted with a %uth order Hilbert Curve.", ip_string_buffer_a, ip_string_buffer_b, hilbert_curve->get_order());
  png_text_array.push_back
    (
      {
        .compression = PNG_TEXT_COMPRESSION_NONE,
        .key = description_key,
        .text = description_text,
        .text_length = strlen(description_text),
        .itxt_length = 0,
        .lang = nullptr,
        .lang_key = nullptr,
      }
    );
  char software_key[]   = "Software";
  char software_text[]  =  PROJECT_NAME " " PROJECT_VER " <" PROJECT_URL ">";
  png_text_array.push_back
    (
      {
        .compression = PNG_TEXT_COMPRESSION_NONE,
        .key = software_key,
        .text = software_text,
        .text_length = strlen(software_text),
        .itxt_length = 0,
        .lang = nullptr,
        .lang_key = nullptr,
      }
    );
  char time_key[] = "Creation Time";
  char time_text[PNG_TEXT_BUFFER_SIZE];
  strftime(time_text, sizeof(time_text), "%a, %d %b %y %T %Z", &gmtime_now);
  png_text_array.push_back
    (
      {
        .compression = PNG_TEXT_COMPRESSION_NONE,
        .key = time_key,
        .text = time_text,
        .text_length = strlen(time_text),
        .itxt_length = 0,
        .lang = nullptr,
        .lang_key = nullptr,
      }
    );
  char author_key[] = "Author";
  char author_text[IMAGE_STRING_BUFFER_SIZE];
  char copyright_key[]  = "Copyright";
  char copyright_text[2*IMAGE_STRING_BUFFER_SIZE];
  if(PINGO_ARGUMENT_VALID == png_config->image_args.hilbert_image_author_status)
  {
    strncpy(author_text, png_config->image_args.hilbert_image_author, sizeof(author_text));
    png_text_array.push_back
      (
        (png_text)
        {
          .compression = PNG_TEXT_COMPRESSION_NONE,
          .key = author_key,
          .text = author_text,
          .text_length = strlen(png_config->image_args.hilbert_image_author),
          .itxt_length = 0,
          .lang = nullptr,
          .lang_key = nullptr,
        }
      );
    snprintf(copyright_text, sizeof(copyright_text), "Copyright %u %s.  All rights reserved.", (YEAR_1900+gmtime_now.tm_year), png_config->image_args.hilbert_image_author);
    png_text_array.push_back
      (
        {
          .compression = PNG_TEXT_COMPRESSION_NONE,
          .key = copyright_key,
          .text = copyright_text,
          .text_length = strlen(copyright_text),
          .itxt_length = 0,
          .lang = nullptr,
          .lang_key = nullptr,
        }
      );
  }

  png_set_text(png_ptr, png_info_ptr, png_text_array.data(), (int)png_text_array.size());
}

void write_png(const png_config_s* png_config, hilbert_curve_c *hilbert_curve)
{
  assert(hilbert_curve != nullptr);

  assert(nullptr == png_ptr);
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, png_error_handler, nullptr);
  if(png_ptr != nullptr)
  {
    assert(nullptr == png_info_ptr);
    png_info_ptr = png_create_info_struct(png_ptr);

    if(png_info_ptr != nullptr)
    {
      printf("Initializing PNG file.\n");
      png_init_io(png_ptr, file_ptr);
      png_set_write_status_fn(png_ptr, png_write_row_cb);

      printf("Filling PNG header info.\n");
      png_set_IHDR( png_ptr, png_info_ptr, 
                    max_coordinate, max_coordinate, 
                    (int)png_config->color_depth, 
                    PNG_COLOR_TYPE_PALETTE, 
                    PNG_INTERLACE_NONE, 
                    PNG_COMPRESSION_TYPE_DEFAULT, 
                    PNG_FILTER_TYPE_DEFAULT );

      fill_png_palette(png_config);

      printf("Filling PNG timestamp.\n");
      time_t time_now = time(NULL);
      struct tm gmtime_now = *gmtime(&time_now);
      png_time mod_time;
      png_convert_from_struct_tm(&mod_time, &gmtime_now);
      png_set_tIME(png_ptr, png_info_ptr, &mod_time);

      fill_png_text(png_config, gmtime_now, hilbert_curve);

      printf("Writing PNG header info.\n");
      png_write_info(png_ptr, png_info_ptr);

      printf("Writing PNG image.\n");
      png_set_packswap(png_ptr);
      png_write_image(png_ptr, row_pointers);

      printf("Writing PNG end.\n");
      png_write_end(png_ptr, nullptr);

      printf("Done writing PNG data.\n");
      cleanup_png_data();
    }
    else
    {
      fprintf(stderr, "Error initializing ping info struct.\n");
      cleanup_png_data();
      safe_exit(1);
    }
  }
  else
  {
    fprintf(stderr, "Error initializing ping write struct.\n");
    cleanup_png_data();
    safe_exit(1);
  }

}

void sandor_laboratories::pingo::generate_png_image(const png_config_s* png_config)
{
  if((png_config != nullptr) && (png_config->file_manager != nullptr))
  {
    if(PINGO_ARGUMENT_VALID == png_config->image_args.hilbert_image_order_status &&
      png_config->image_args.hilbert_image_order >   0 &&
      png_config->image_args.hilbert_image_order <= HILBERT_ORDER_FOR_32_BITS)
    {
      hilbert_curve_c hilbert_curve(png_config->image_args.hilbert_image_order);
      assert(0 == max_coordinate);
      max_coordinate = hilbert_curve.max_coordinate();
      allocate_image_memory(png_config);

      const hilbert_image_from_file_params_s hilbert_image_from_file_params = 
        {
          .hilbert_curve = &hilbert_curve,
          .png_config    = png_config,
          .row_pointers  = row_pointers,
        };
      png_config->file_manager->iterate_file_registry(fill_hilbert_image_from_file, 
                                                     &hilbert_image_from_file_params, 
                                                      png_config->initial_ip, hilbert_curve.max_index()) ;

      if(PINGO_ARGUMENT_VALID == png_config->image_args.annotate_status)
      {
        printf("Annotating image.\n");
        annotate_hilbert_image(&hilbert_image_from_file_params);
      }

      printf("Opening file %s for writing.\n", png_config->image_file_path);
      assert(nullptr == file_ptr);
      file_ptr = fopen(png_config->image_file_path, "wb");
      if(file_ptr != nullptr)
      {
        write_png(png_config, &hilbert_curve);
      }
      else
      {
        fprintf(stderr, "Error opening image file '%s' for writing.  errno %u: %s\n", png_config->image_file_path, errno, strerror(errno));
        cleanup_png_data();
        safe_exit(1);
      }
    }
    else
    {
      fprintf(stderr, "Hilbert Order config invalid.  status %u order %u\n", 
        png_config->image_args.hilbert_image_order_status,
        png_config->image_args.hilbert_image_order);
      cleanup_png_data();
      safe_exit(1);
    }
  }
  else
  {
    fprintf(stderr, "Null input to generate image.  png_config %p file_manager %p\n", 
      png_config, ((png_config != nullptr)?png_config->file_manager:0));
    cleanup_png_data();
    safe_exit(1);
  }
}