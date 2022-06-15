#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <png.h>

#include "file.hpp"
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

void fill_hilbert_image_from_file(const file_s* file, const void * user_data_ptr)
{
  assert(file);
  assert(user_data_ptr);
  char ip_string_buffer[IP_STRING_SIZE];

  const hilbert_image_from_file_params_s * params = (const hilbert_image_from_file_params_s *) user_data_ptr;

  ip_string(file->header.first_address, ip_string_buffer, sizeof(ip_string_buffer));
  printf("Filling image data for file starting at IP %s with %u IPs.\n", ip_string_buffer, file->header.address_count);

  const hilbert_index_t      max_index       = params->hilbert_curve->max_index();
  const hilbert_coordinate_t max_coordinate  = params->hilbert_curve->max_coordinate();
  const uint_fast64_t        last_ip         = ((uint_fast64_t)params->png_config->initial_ip) + max_index;
  const uint_fast64_t        file_last_ip    = file->header.first_address + file->header.address_count;
  const unsigned int         pixels_per_byte = (8/params->png_config->color_depth);

  for(uint_fast64_t i = MAX(params->png_config->initial_ip, file->header.first_address); i < MIN(last_ip, file_last_ip); i++)
  {
    const file_data_entry_s * file_data_entry = &file->data[(i-file->header.first_address)]; 
    if(FILE_DATA_ENTRY_ECHO_REPLY == file_data_entry->type)
    {
      const hilbert_index_t hilbert_index = (i-params->png_config->initial_ip);
      hilbert_coordinate_s  coordinate;
      assert(params->hilbert_curve->get_coordinate(hilbert_index, &coordinate));

      const png_bytep  row_pointer    =  params->row_pointers[coordinate.y];
      png_byte        *column_pointer = &row_pointer[(coordinate.x%(max_coordinate))/8];

      *column_pointer |= (1 << (coordinate.x%pixels_per_byte));
    //  printf("(%u, %u) - %p - 0x%x\n", coordinate.x, coordinate.y, column_pointer, *column_pointer);
    }
  }
}

FILE * fp = nullptr;
png_structp png_ptr = nullptr;
png_infop png_info_ptr = nullptr;
hilbert_coordinate_t max_coordinate = 0;
png_bytep row_pointers[65536] = {0};

void sandor_laboratories::pingo::init_png_config(png_config_s* png_config)
{
  if(png_config)
  {
    memset(png_config, 0, sizeof(png_config_s));
    png_config->color_depth = 1;
  }
}

void cleanup_png_data()
{
  if(fp)
  {
    printf("Closing file.\n");
    fclose(fp);
  }
  printf("Destroying PNG data.\n");
  png_destroy_write_struct(&png_ptr, &png_info_ptr);

  printf("Freeing image memory.\n");
  for(unsigned int i = 0; i < max_coordinate; i++)
  {
    if(row_pointers[i])
    {
      free(row_pointers[i]);
    }
  }
  max_coordinate = 0;
}

void png_error_handler(png_structp, png_const_charp error_string)
{
  fprintf(stderr, "libpng error - %s.\n", error_string);
  cleanup_png_data();
  safe_exit(1);
}

void sandor_laboratories::pingo::generate_png_image(const png_config_s* png_config)
{
  if(png_config && png_config->file_manager)
  {
    if(PINGO_ARGUMENT_VALID == png_config->image_args.hilbert_image_order_status &&
      png_config->image_args.hilbert_image_order >   0 &&
      png_config->image_args.hilbert_image_order <= 16)
    {
      hilbert_curve_c hilbert_curve(png_config->image_args.hilbert_image_order);
      assert(0 == max_coordinate);
      max_coordinate = hilbert_curve.max_coordinate();
      const unsigned int pixels_per_byte = (8/png_config->color_depth);
      printf("Allocating image memory for %u x %u PNG image.  %u pixels_per_byte\n", max_coordinate, max_coordinate, pixels_per_byte);
      for(unsigned int i = 0; i < max_coordinate; i++)
      {
        row_pointers[i] = (png_byte*) calloc(sizeof(png_byte), max_coordinate/pixels_per_byte);
      }

      const hilbert_image_from_file_params_s hilbert_image_from_file_params = 
        {
          .hilbert_curve = &hilbert_curve,
          .png_config    = png_config,
          .row_pointers  = row_pointers,
        };
      png_config->file_manager->iterate_file_registry(fill_hilbert_image_from_file, 
                                                     &hilbert_image_from_file_params, 
                                                      png_config->initial_ip, hilbert_curve.max_index()) ;

      printf("Opening file %s for writing.\n", png_config->image_file_path);
      assert(nullptr == fp);
      fp = fopen(png_config->image_file_path, "wb");
      if(fp)
      {
        assert(nullptr == png_ptr);
        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, png_error_handler, nullptr);
        if(png_ptr)
        {
          assert(nullptr == png_info_ptr);
          png_info_ptr = png_create_info_struct(png_ptr);

          if(png_info_ptr)
          {
            printf("Initializing PNG file.\n");
            png_init_io(png_ptr, fp);

            printf("Filling PNG header info.\n");
            png_set_IHDR( png_ptr, png_info_ptr, 
                          max_coordinate, max_coordinate, 
                          png_config->color_depth, 
                          PNG_COLOR_TYPE_GRAY, 
                          PNG_INTERLACE_NONE, 
                          PNG_COMPRESSION_TYPE_DEFAULT, 
                          PNG_FILTER_TYPE_DEFAULT );

            printf("Filling PNG timestamp.\n");
            time_t time_now = time(NULL);
            struct tm gmtime_now = *gmtime(&time_now);
            png_time mod_time;
            png_convert_from_struct_tm(&mod_time, &gmtime_now);
            png_set_tIME(png_ptr, png_info_ptr, &mod_time);
 
            printf("Filling PNG text info.\n");
            
            std::vector<png_text> png_text_array;
            char title_key[]      = "Title";
            char title_text[1024];
            char ip_string_buffer_a[IP_STRING_SIZE];
            char ip_string_buffer_b[IP_STRING_SIZE];
            ip_string(png_config->initial_ip, ip_string_buffer_a, sizeof(ip_string_buffer_a));
            ip_string((((uint_fast64_t) png_config->initial_ip)+hilbert_curve.max_index())-1, ip_string_buffer_b, sizeof(ip_string_buffer_a));
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
            char description_text[1024];
            ip_string(png_config->initial_ip, ip_string_buffer_a, sizeof(ip_string_buffer_a));
            ip_string((((uint_fast64_t) png_config->initial_ip)+hilbert_curve.max_index())-1, ip_string_buffer_b, sizeof(ip_string_buffer_a));
            snprintf(description_text, sizeof(description_text), "ICMP echo replies for IP addresses %s - %s plotted with a %uth order Hilbert Curve.", ip_string_buffer_a, ip_string_buffer_b, hilbert_curve.get_order());
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
            char time_text[1024];
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
              snprintf(copyright_text, sizeof(copyright_text), "Copyright %u %s.  All rights reserved.", (1900+gmtime_now.tm_year), png_config->image_args.hilbert_image_author);
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

            png_set_text(png_ptr, png_info_ptr, &png_text_array[0], png_text_array.size());

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
      png_config, (png_config?png_config->file_manager:0));
    cleanup_png_data();
    safe_exit(1);
  }
}