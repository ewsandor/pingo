#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <openssl/evp.h>

#include "file.hpp"
#include "pingo.hpp"

using namespace sandor_laboratories::pingo;

file_manager_c::file_manager_c(const char * wd)
{
  assert(wd);
  strncpy(working_directory, wd, sizeof(working_directory));
  mdctx = EVP_MD_CTX_new();
}

file_manager_c::~file_manager_c()
{
  EVP_MD_CTX_free(mdctx);
}

bool file_manager_c::file_header_valid(const file_s* file)
{
  bool ret_val = true;

  if(file)
  {
    ret_val = ( (FILE_SIGNATURE == file->header.signature) &&
                (FILE_VERSION_0 == file->header.version) );
  }
  else
  {
    fprintf(stderr, "Null file pointer 0x%p\n", file);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::read_file(const char * file_path, file_s* output_file, bool skip_data)
{
  bool ret_val = true;
  FILE * fp;

  if(file_path && output_file)
  {
    if((fp = fopen(file_path, "rb")))
    {
      if(read_file_header(fp, output_file))
      {
        if(!skip_data)
        {
          if(!read_file_data(fp, output_file))
          {
            fprintf(stderr, "Failed to read data for file '%s'.\n", file_path);
            ret_val = false;
          }
        }
        else
        {
          if(0 != fseek(fp, sizeof(file_data_entry_s)*output_file->header.address_count, SEEK_CUR))
          {
            fprintf(stderr, "Failed to seek past data for file '%s'.  errno %u: %s\n", file_path, errno, strerror(errno));
          }
        }

        if(ret_val && !read_file_checksum(fp, output_file))
        {
          fprintf(stderr, "Failed to read checksum for file '%s'.\n", file_path);
          ret_val = false;
        }
      }
      else
      {
        fprintf(stderr, "Failed to read header for file '%s'.\n", file_path);
        ret_val = false;
      }
      assert(0 == fclose(fp));
    }
    else
    {
      fprintf(stderr, "Failed to open file '%s' for reading.  errno %u: %s\n", file_path, errno, strerror(errno));
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null file_path 0x%p or output_file 0x%p.\n", file_path, output_file);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::read_file_header(FILE * fp, file_s* output_file)
{
  bool ret_val = true;
  char ip_string_buffer[IP_STRING_SIZE];

  if(fp && output_file)
  {
    if(1 == fread(&output_file->header, sizeof(output_file->header), 1, fp))
    {
      if(!file_header_valid(output_file))
      {
        ip_string(output_file->header.first_address, ip_string_buffer, sizeof(ip_string_buffer));
        fprintf(stderr, "Invalid file header.  signature 0x%lx version %u first_address %s address_count %u\n", 
          output_file->header.signature,
          output_file->header.version,
          ip_string_buffer,
          output_file->header.address_count);
        ret_val = false;
      }
    }
    else
    {
      fprintf(stderr, "Failed to read file header.  feof %d ferror %d\n", feof(fp), ferror(fp));
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null file_stream 0x%p or output_file 0x%p.\n", fp, output_file);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::read_file_data(FILE * fp, file_s* output_file)
{
  bool ret_val = true;
  char ip_string_buffer[IP_STRING_SIZE];

  if(fp && output_file)
  {
    if(file_header_valid(output_file))
    {
      output_file->data = (file_data_entry_s*) malloc(sizeof(file_data_entry_s)*output_file->header.address_count);

      if(output_file->data)
      {
        if(output_file->header.address_count !=
           fread(output_file->data, sizeof(file_data_entry_s), output_file->header.address_count, fp))
        {
          free(output_file->data);
          output_file->data = nullptr;
          fprintf(stderr, "Failed to read file data.  feof %d ferror %d\n", feof(fp), ferror(fp));
          ret_val = false;
        }
      }
      else
      {
        fprintf(stderr, "Failed to allocate memory for file data\n");
        ret_val = false;
      }
    }
    else
    {
      ip_string(output_file->header.first_address, ip_string_buffer, sizeof(ip_string_buffer));
      fprintf(stderr, "Invalid file header.  signature 0x%lx version %u first_address %s address_count %u\n", 
        output_file->header.signature,
        output_file->header.version,
        ip_string_buffer,
        output_file->header.address_count);
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null file_stream 0x%p or output_file 0x%p.\n", fp, output_file);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::read_file_checksum(FILE * fp, file_s* output_file)
{
  bool ret_val = true;

  if(fp && output_file)
  {
    if(1 != fread(&output_file->checksum, sizeof(output_file->checksum), 1, fp))
    {
      fprintf(stderr, "Failed to read file header.  feof %d ferror %d\n", feof(fp), ferror(fp));
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null file_stream 0x%p or output_file 0x%p.\n", fp, output_file);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::verify_checksum(const file_s* file, EVP_MD_CTX * mdctx)
{
  bool ret_val = true;
  file_checksum_t checksum;

  if(file)
  {
    if(generate_file_checksum(file, checksum, mdctx))
    {
      ret_val = (0 == memcmp(checksum, file->checksum, sizeof(file_checksum_t)));
    }
    else
    {
      fprintf(stderr, "Failed to generate checksum\n");
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null file pointer 0x%p.\n", file);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::verify_checksum(const file_s* file)
{
  return verify_checksum(file, mdctx);
}

bool file_manager_c::generate_file_checksum(const file_s *file, file_checksum_t checksum, EVP_MD_CTX * mdctx)
{
  bool ret_val = true;
  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;

  if(file && checksum)
  {
    /* Reset checksum context */
    EVP_DigestInit_ex(mdctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(mdctx, &file->header, sizeof(file->header));
    EVP_DigestUpdate(mdctx, file->data, sizeof(file_data_entry_s)*file->header.address_count);
    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    assert(sizeof(file_checksum_t) == md_len);
    memcpy(checksum, md_value, sizeof(file_checksum_t));
  }
  else
  {
    fprintf(stderr, "Null file (0x%p) or checksum (0x%p) pointer.\n", file, checksum);
    ret_val = false;
  }

  return ret_val;
}
bool file_manager_c::generate_file_checksum(const file_s *file, file_checksum_t checksum)
{
  return generate_file_checksum(file, checksum, mdctx);
}

bool file_manager_c::file_path_from_directory_filename(const char * directory, const char * filename, char * path, size_t path_buffer_size)
{
  bool ret_val = true;

  if(directory && filename && path && (path_buffer_size > 0))
  {
    if(0 >= snprintf(path, path_buffer_size, "%s/%s", directory, filename))
    {
      fprintf(stderr, "Failed to build full path.  directory %s filename %s path %s\n", directory, filename, path);
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null directory (0x%p) filename (0x%p) or path (0x%p) pointer or invalid buffer_size (%lu).\n", 
      directory, filename, path, path_buffer_size);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::write_ping_block_to_file(ping_block_c* ping_block)
{
  bool   ret_val = true;
  uint_fast32_t i;
  char   ip_string_buffer[IP_STRING_SIZE];
  char   filename[FILE_NAME_MAX_LENGTH];
  char   path[FILE_PATH_MAX_LENGTH];
  file_s file;
  FILE * fp;
  ping_block_entry_s ping_block_entry;

  if(ping_block && (ping_block->get_address_count() > 0))
  {
    ip_string(ping_block->get_first_address(), ip_string_buffer, sizeof(ip_string_buffer), '_', true);
    snprintf(filename, sizeof(filename), "%s.pingo", ip_string_buffer);
    if(file_path_from_directory_filename(working_directory, filename, path, sizeof(path)))
    {
      /* Construct file */
      memset(&file, 0, sizeof(file));
      /* Build header */
      file.header.signature     = FILE_SIGNATURE;
      file.header.version       = FILE_VERSION_0;
      file.header.first_address = ping_block->get_first_address();
      file.header.address_count = ping_block->get_address_count();
      /* Fill data */
      file.data = (file_data_entry_s*) calloc(sizeof(file_data_entry_s), file.header.address_count);
      for(i = 0; i < ping_block->get_address_count(); i++)
      {
        assert(ping_block->get_ping_block_entry((ping_block->get_first_address()+i), &ping_block_entry));
        if(ping_block_entry.reply_valid)
        {
          file.data[i].type = FILE_DATA_ENTRY_ECHO_REPLY;
          file.data[i].payload.echo_reply.reply_time = 
            ((ping_block_entry.ping_time < FILE_ECHO_REPLY_TIME_MAX)?ping_block_entry.ping_time:FILE_ECHO_REPLY_TIME_MAX);
        }
        else
        {
          file.data[i].type = FILE_DATA_ENTRY_ECHO_NO_REPLY;
          file.data[i].payload.echo_reply.reply_time = FILE_ECHO_REPLY_TIME_MAX;
        }
      }
      /* Fill checksum */
      generate_file_checksum(&file, file.checksum);

      block_exit(EXIT_BLOCK_WRITE_FILE_OPEN);
      if((fp = fopen(path, "wb")))
      {
        assert(1 == fwrite(&file.header,  sizeof(file.header), 1, fp));
        assert(file.header.address_count == 
                    fwrite(file.data,     sizeof(file_data_entry_s), file.header.address_count, fp));
        assert(1 == fwrite(file.checksum, sizeof(file.checksum), 1, fp));
        assert(0 == fclose(fp));
      }
      else
      {
        fprintf(stderr, "Failed to open file '%s' for writing.  errno %u: %s\n", filename, errno, strerror(errno));
        ret_val = false;
      }
      unblock_exit(EXIT_BLOCK_WRITE_FILE_OPEN);

      free(file.data);
    }
    else
    {
      fprintf(stderr, "Failed to build file path (%s)\n", path);
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Invalid ping block passed for writing to file.  pointer %p address_count %d\n", 
      ping_block, (ping_block?ping_block->get_address_count():-1));
    ret_val = false;
  }

  return ret_val;
}