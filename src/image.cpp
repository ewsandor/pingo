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
  UNUSED(params);

  ip_string(file->header.first_address, ip_string_buffer, sizeof(ip_string_buffer));
  printf("Filling PNG for file starting at IP %s with %u IPs.\n", ip_string_buffer, file->header.address_count);
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
      unsigned int pixels_per_byte = (8/png_config->color_depth);
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

            printf("Filling PNG text info.\n");
            
            char title_key[]      = "Title";
            char title_text[]     = "Ping Reply Hilbert Curve";
            char author_key[]     = "Author";
            char author_text[]    = "Edward Sandor";
            char copyright_key[]  = "Copyright";
            char copyright_text[] = "Copyright 2022 Edward Sandor.  All rights reserved.";
            char software_key[]   = "Software";
            char software_text[]  =  PROJECT_NAME " " PROJECT_VER " <" PROJECT_URL ">";
            png_text png_text_array[] = 
            {
              {
                .compression = PNG_TEXT_COMPRESSION_NONE,
                .key = title_key,
                .text = title_text,
                .text_length = strlen(title_text),
                .itxt_length = 0,
                .lang = nullptr,
                .lang_key = nullptr,
              },
              {
                .compression = PNG_TEXT_COMPRESSION_NONE,
                .key = author_key,
                .text = author_text,
                .text_length = strlen(author_text),
                .itxt_length = 0,
                .lang = nullptr,
                .lang_key = nullptr,
              },
              {
                .compression = PNG_TEXT_COMPRESSION_NONE,
                .key = copyright_key,
                .text = copyright_text,
                .text_length = strlen(copyright_text),
                .itxt_length = 0,
                .lang = nullptr,
                .lang_key = nullptr,
              },
              {
                .compression = PNG_TEXT_COMPRESSION_NONE,
                .key = software_key,
                .text = software_text,
                .text_length = strlen(software_text),
                .itxt_length = 0,
                .lang = nullptr,
                .lang_key = nullptr,
              },
            };

            png_set_text(png_ptr, png_info_ptr, png_text_array, (sizeof(png_text_array)/sizeof(png_text)));

            printf("Writing PNG header info.\n");
            png_write_info(png_ptr, png_info_ptr);

            printf("Writing PNG image.\n");
            //png_set_packswap(png_ptr);
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