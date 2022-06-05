#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <openssl/evp.h>

#include "file.hpp"
#include "pingo.hpp"

using namespace sandor_laboratories::pingo;

static const file_manager_config_s default_file_manager_config = 
  {
    .verbose = false,
  };

file_manager_c::file_manager_c(const char * wd)
  : config(default_file_manager_config)
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
                (FILE_VERSION_0 == file->header.version) && 
                (file->header.address_count > 0));
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
    if (0 != ftell(fp))
    {
      fseek(fp, 0, SEEK_SET);
    }

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
      if (sizeof(output_file->header) != ftell(fp))
      {
        fseek(fp, sizeof(output_file->header), SEEK_SET);
      }
      
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
      fprintf(stderr, "Invalid file header to read data.  signature 0x%lx version %u first_address %s address_count %u\n", 
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
  char ip_string_buffer[IP_STRING_SIZE];

  if(fp && output_file)
  {
    if(file_header_valid(output_file))
    {
      if ((sizeof(output_file->header)+(sizeof(file_data_entry_s)*output_file->header.address_count)) != ((unsigned long) ftell(fp)))
      {
        fseek(fp, (sizeof(output_file->header)+(sizeof(file_data_entry_s)*output_file->header.address_count)), SEEK_SET);
      }

      if(1 != fread(&output_file->checksum, sizeof(output_file->checksum), 1, fp))
      {
        fprintf(stderr, "Failed to read file header.  feof %d ferror %d\n", feof(fp), ferror(fp));
        ret_val = false;
      }
    }
    else
    {
      ip_string(output_file->header.first_address, ip_string_buffer, sizeof(ip_string_buffer));
      fprintf(stderr, "Invalid file header to read checksum.  signature 0x%lx version %u first_address %s address_count %u\n", 
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

bool file_manager_c::delete_file_data(file_s* file)
{
  bool ret_val = true;

  if(file)
  {
    if(file->data)
    {
      free(file->data);
      file->data = nullptr;
    }
  }
  else
  {
    fprintf(stderr, "Null file pointer for delete\n");
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

      delete_file_data(&file);
      add_file_to_registry(filename, &file, FILE_REGISTRY_ENTRY_READ_HEADER_ONLY);
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

void file_manager_c::sort_registry()
{
  unsigned int i,j;
  registry_entry_s swap_buffer;
  unsigned int valid_entries = 0; 

  /* Insertion sort */
  for(i = 1; i < registry.size(); i++)
  {
    if(FILE_REGISTRY_READ_AND_VALID(registry[i].state))
    {
      valid_entries++;
      if( (!FILE_REGISTRY_READ_AND_VALID(registry[i-1].state)) ||
          (registry[i].file.header.first_address < registry[i-1].file.header.first_address) )
      {
        swap_buffer = registry[i];
        for(j = i; j > 0; j--)
        {
          if( (FILE_REGISTRY_READ_AND_VALID(registry[j-1].state)) &&
              (swap_buffer.file.header.first_address > registry[j-1].file.header.first_address) )
          {
            break;
          }
          else
          {
            registry[j] = registry[j-1];
          }
        }
        registry[j] = swap_buffer;
      }
    }
  }
  registry.resize(valid_entries);
}

bool file_manager_c::add_file_to_registry(const char * file_name, const file_s* file, registry_entry_state_e state)
{
  bool ret_val = true;
  registry_entry_s  registry_entry;

  if(file_name && file && (state < FILE_REGISTRY_ENTRY_MAX))
  {
    memset(&registry_entry, 0, sizeof(registry_entry_s));
    strncpy(registry_entry.file_name, file_name, sizeof(registry_entry.file_name));
    registry_entry.file = *file;
    registry_entry.state = state;
    registry.push_back(registry_entry);
  }
  else
  {
    fprintf(stderr, "Bad input to add file to registry.  file_name %s file %p state %u\n", (file_name?file_name:"(null)"), file, state);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::build_registry()
{
  bool              ret_val = true;
  char              file_path[FILE_PATH_MAX_LENGTH];
  char              ip_string_buffer[IP_STRING_SIZE];
  struct dirent    *dp;
  DIR              *dir;
  file_s            file;

  dir = opendir(working_directory);

  if(dir)
  {
    errno = 0;

    while((dp = readdir(dir)))
    {
      file_path_from_directory_filename(working_directory, dp->d_name, file_path, sizeof(file_path));

      if(read_file(file_path, &file, true) && file_header_valid(&file))
      {
        add_file_to_registry(dp->d_name, &file, FILE_REGISTRY_ENTRY_READ_HEADER_ONLY);
        if(config.verbose)
        {
          ip_string(file.header.first_address, ip_string_buffer, sizeof(ip_string_buffer));
          printf("Found pingo file '%s' for ping block starting at IP %s with %u addresses\n", 
            dp->d_name, ip_string_buffer, file.header.address_count);
        }
      }
      else
      {
        add_file_to_registry(dp->d_name, &file, FILE_REGISTRY_ENTRY_INVALID_HEADER);
      }

      errno = 0;
    }

    if(errno != 0)
    {
      fprintf(stderr, "Error reading directory.  errno %u: %s\n", errno, strerror(errno));
      ret_val = false;
    }

    closedir(dir);
  }
  else
  {
    fprintf(stderr, "Failed to open directory '%s' to build registry.  errno %u: %s\n", 
      working_directory, errno, strerror(errno));
    ret_val = false;
  }

  sort_registry();

  return ret_val;
}

uint32_t file_manager_c::get_next_registry_hole_ip()
{
  uint32_t ret_val = 0;
  std::vector<registry_entry_s>::iterator it;

  sort_registry();

  for(it = registry.begin(); it != registry.end(); it++)
  {
    if(FILE_REGISTRY_READ_AND_VALID(it->state))
    {
      if( (ret_val >= it->file.header.first_address) && 
          (ret_val < (it->file.header.first_address + it->file.header.address_count)) )
      {
        ret_val = (it->file.header.first_address + it->file.header.address_count);
      }
      else
      {
        break;
      }
    }
    else
    {
      fprintf(stderr, "File registry %s in unexpected state %u when getting next registry hole.", it->file_name, it->state);
    }
  }

  return ret_val;
}

bool file_manager_c::validate_files_in_registry()
{
  bool ret_val = true;
  std::vector<registry_entry_s>::iterator it;
  uint32_t last_file_last_ip  = 0;
  char     file_path[FILE_PATH_MAX_LENGTH];
  char     ip_string_buffer_a[IP_STRING_SIZE];
  char     ip_string_buffer_b[IP_STRING_SIZE];

  sort_registry();

  for(it = registry.begin(); it != registry.end(); it++)
  {
    if(FILE_REGISTRY_VALID_HEADER(it->state))
    {
      file_path_from_directory_filename(working_directory, it->file_name, file_path, sizeof(file_path));

      read_file(file_path, &it->file);
      it->state = (verify_checksum(&it->file)?FILE_REGISTRY_ENTRY_READ_HEADER_ONLY_VALIDATED:FILE_REGISTRY_ENTRY_CORRUPTED);
      delete_file_data(&it->file);

      if(it->file.header.first_address > (last_file_last_ip+1))
      {
        ip_string((last_file_last_ip+1),         ip_string_buffer_a, sizeof(ip_string_buffer_a));
        ip_string((it->file.header.first_address-1), ip_string_buffer_b, sizeof(ip_string_buffer_b));
        printf("No data for IPs %s - %s\n", ip_string_buffer_a, ip_string_buffer_b);
        ret_val = false;
      }

      ip_string(it->file.header.first_address, ip_string_buffer_a, sizeof(ip_string_buffer_a));
      ip_string(((it->file.header.first_address+it->file.header.address_count)-1), ip_string_buffer_b, sizeof(ip_string_buffer_b));
      if(FILE_REGISTRY_ENTRY_CORRUPTED == it->state)
      {
        printf("CORRUPTED PINGO FILE '%s' FOR IPs %s - %s!\n", it->file_name, ip_string_buffer_a, ip_string_buffer_b);
        ret_val = false;
      }
      else
      {
        printf("Pingo file '%s' for IPs %s - %s validated.\n", it->file_name, ip_string_buffer_a, ip_string_buffer_b);
        last_file_last_ip  = (it->file.header.first_address + it->file.header.address_count)-1;
      }
    }
  }

  if(last_file_last_ip < 0xFFFFFFFF)
  {
    ip_string((last_file_last_ip+1),         ip_string_buffer_a, sizeof(ip_string_buffer_a));
    ip_string(0xFFFFFFFF, ip_string_buffer_b, sizeof(ip_string_buffer_b));
    printf("No data for IPs %s - %s\n", ip_string_buffer_a, ip_string_buffer_b);
    ret_val = false;
  }



  return ret_val;
}