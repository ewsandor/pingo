#include <cassert>
#include <cstdio>
#include <cstring>
#include <png.h>

#include "image.hpp"
#include "pingo.hpp"
#include "version.hpp"

using namespace sandor_laboratories::pingo;

const char * test_image_path = "test.png";

FILE * fp = nullptr;
png_structp png_ptr = nullptr;
png_infop png_info_ptr = nullptr;

void cleanup_png_data()
{
  if(fp)
  {
    fclose(fp);
  }
  png_destroy_write_struct(&png_ptr, &png_info_ptr);
}

void png_error_handler(png_structp, png_const_charp error_string)
{
  fprintf(stderr, "libpng error - %s.\n", error_string);
  cleanup_png_data();
  safe_exit(1);
}

void generate_png_image()
{
  assert(nullptr == fp);
  assert(nullptr == png_ptr);
  assert(nullptr == png_info_ptr);

  fp = fopen(test_image_path, "wb");

  if(fp)
  {
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, png_error_handler, nullptr);
    if(png_ptr)
    {
      png_info_ptr = png_create_info_struct(png_ptr);

      if(png_info_ptr)
      {
        printf("Initializing PNG IO.\n");
        png_init_io(png_ptr, fp);

        printf("Filling PNG header info.\n");
        png_set_IHDR( png_ptr, png_info_ptr, 256, 256, 1, 
                      PNG_COLOR_TYPE_GRAY, 
                      PNG_INTERLACE_NONE, 
                      PNG_COMPRESSION_TYPE_DEFAULT, 
                      PNG_FILTER_TYPE_DEFAULT );

        printf("Filling PNG text info.\n");
        
        char title_key[]      = "Title";
        char title_text[]     = "Pingo PNG Test Image";
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

        printf("Writing PNG info.\n");
        png_write_info(png_ptr, png_info_ptr);

        printf("Writing PNG image.\n");
        png_bytep row_pointers[256];
        for(unsigned int i = 0; i < 256; i++)
        {
          row_pointers[i] = (png_bytep) malloc(png_get_rowbytes(png_ptr, png_info_ptr));
          memset(row_pointers[i], 0, png_get_rowbytes(png_ptr, png_info_ptr));
          for(unsigned int j = 0; j < png_get_rowbytes(png_ptr, png_info_ptr); j++)
          {
            row_pointers[i][j] = i+j;
          }
        }
        printf("Row size %lu\n", png_get_rowbytes(png_ptr, png_info_ptr));
        png_write_image(png_ptr, row_pointers);

        printf("Writing PNG end.\n");
        png_write_end(png_ptr, nullptr);

        printf("Closing file and cleaning up PNG data.\n");
        cleanup_png_data();
      
        for(unsigned int i; i < 256; i++)
        {
          free(row_pointers[i]);
        }
 
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
    fprintf(stderr, "Error opening image file '%s' for writing.\n", test_image_path);
    cleanup_png_data();
    safe_exit(1);
  }
}