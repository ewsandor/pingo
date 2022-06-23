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
    .stats_on_validation = true,
  };

file_manager_c::file_manager_c(const char * working_directory_)
  : config(default_file_manager_config)
{
  assert(working_directory_);
  strncpy(working_directory, working_directory_, sizeof(working_directory));
  mdctx = EVP_MD_CTX_new();
}

file_manager_c::~file_manager_c()
{
  EVP_MD_CTX_free(mdctx);
}

bool file_manager_c::file_header_valid(const file_s* file)
{
  bool ret_val = true;

  if(file != nullptr)
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

bool file_manager_c::file_data_valid(const file_s* file)
{
  bool ret_val = true;

  if(file != nullptr)
  {
    ret_val = (file_header_valid(file) && (file->data != nullptr));
  }
  else
  {
    fprintf(stderr, "Null file pointer 0x%p\n", file);
    ret_val = false;
  }

  return ret_val;
}

inline bool file_manager_c::read_remaining_file(const char * file_path, FILE * file_ptr, file_s* output_file, bool skip_data)
{
  bool ret_val = true;

  if(!skip_data)
  {
    if(!read_file_data(file_ptr, output_file))
    {
      fprintf(stderr, "Failed to read data for file '%s'.\n", file_path);
      ret_val = false;
    }
  }
  else
  {
    if(0 != fseek(file_ptr, (long)(sizeof(file_data_entry_s)*output_file->header.address_count), SEEK_CUR))
    {
      fprintf(stderr, "Failed to seek past data for file '%s'.  errno %u: %s\n", file_path, errno, strerror(errno));
    }
  }

  if(ret_val && !read_file_checksum(file_ptr, output_file))
  {
    fprintf(stderr, "Failed to read checksum for file '%s'.\n", file_path);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::read_file(const char * file_path, file_s* output_file, bool skip_data)
{
  bool ret_val = true;
  FILE * file_ptr;

  if((file_path != nullptr) && (output_file != nullptr))
  {
    if((file_ptr = fopen(file_path, "rb")) != nullptr)
    {
      if(read_file_header(file_ptr, output_file))
      {
        ret_val = read_remaining_file(file_path, file_ptr, output_file, skip_data);
      }
      else
      {
        fprintf(stderr, "Failed to read header for file '%s'.\n", file_path);
        ret_val = false;
      }
      assert(0 == fclose(file_ptr));
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

bool file_manager_c::read_file_header(FILE * file_ptr, file_s* output_file)
{
  bool ret_val = true;
  char ip_string_buffer[IP_STRING_SIZE];

  if((file_ptr != nullptr) && (output_file != nullptr))
  {
    if (0 != ftell(file_ptr))
    {
      fseek(file_ptr, 0, SEEK_SET);
    }

    if(1 == fread(&output_file->header, sizeof(output_file->header), 1, file_ptr))
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
      fprintf(stderr, "Failed to read file header.  feof %d ferror %d\n", feof(file_ptr), ferror(file_ptr));
      ret_val = false;
    }
  }
  else
  {
    fprintf(stderr, "Null file_stream 0x%p or output_file 0x%p.\n", file_ptr, output_file);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::read_file_data(FILE * file_ptr, file_s* output_file)
{
  bool ret_val = true;
  char ip_string_buffer[IP_STRING_SIZE];

  if((file_ptr != nullptr) && (output_file != nullptr))
  {
    if(file_header_valid(output_file))
    {
      if (sizeof(output_file->header) != ftell(file_ptr))
      {
        fseek(file_ptr, sizeof(output_file->header), SEEK_SET);
      }
      
      output_file->data = (file_data_entry_s*) malloc(sizeof(file_data_entry_s)*output_file->header.address_count);

      if(output_file->data != nullptr)
      {
        if(output_file->header.address_count !=
           fread(output_file->data, sizeof(file_data_entry_s), output_file->header.address_count, file_ptr))
        {
          free(output_file->data);
          output_file->data = nullptr;
          fprintf(stderr, "Failed to read file data.  feof %d ferror %d\n", feof(file_ptr), ferror(file_ptr));
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
    fprintf(stderr, "Null file_stream 0x%p or output_file 0x%p.\n", file_ptr, output_file);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::read_file_checksum(FILE * file_ptr, file_s* output_file)
{
  bool ret_val = true;
  char ip_string_buffer[IP_STRING_SIZE];

  if((file_ptr != nullptr) && (output_file != nullptr))
  {
    if(file_header_valid(output_file))
    {
      if ((sizeof(output_file->header)+(sizeof(file_data_entry_s)*output_file->header.address_count)) != ((unsigned long) ftell(file_ptr)))
      {
        fseek(file_ptr, (long)(sizeof(output_file->header)+(sizeof(file_data_entry_s)*output_file->header.address_count)), SEEK_SET);
      }

      if(1 != fread(&output_file->checksum, sizeof(output_file->checksum), 1, file_ptr))
      {
        fprintf(stderr, "Failed to read file header.  feof %d ferror %d\n", feof(file_ptr), ferror(file_ptr));
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
    fprintf(stderr, "Null file_stream 0x%p or output_file 0x%p.\n", file_ptr, output_file);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::verify_checksum(const file_s* file, EVP_MD_CTX * mdctx)
{
  bool ret_val = true;
  file_checksum_t checksum;

  if(file != nullptr)
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

  if(file != nullptr)
  {
    if(file->data != nullptr)
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

file_stats_s file_manager_c::get_stats_from_file(const file_s* file)
{
  file_stats_s stats;
  uint64_t mean_accumulator = 0;

  memset(&stats, 0, sizeof(file_stats_s));
  stats.min_reply_time = PINGO_BLOCK_PING_TIME_NO_RESPONSE;

  if((file != nullptr) && file_data_valid(file))
  {
    for(unsigned int i = 0; i < file->header.address_count; i++)
    {
      if(FILE_DATA_ENTRY_ECHO_REPLY == file->data[i].type)
      {
        stats.valid_replies++;
        mean_accumulator += file->data[i].payload.echo_reply.reply_time;
        if(file->data[i].payload.echo_reply.reply_time < stats.min_reply_time)
        {
          stats.min_reply_time = file->data[i].payload.echo_reply.reply_time;
        }
        if(file->data[i].payload.echo_reply.reply_time > stats.max_reply_time)
        {
          stats.max_reply_time = file->data[i].payload.echo_reply.reply_time;
        }
      }
      else if(FILE_DATA_ENTRY_ECHO_SKIPPED == file->data[i].type)
      {
        stats.echos_skipped++;
      }
    }
  }

  if(stats.valid_replies > 0)
  {
    mean_accumulator /= stats.valid_replies;
    stats.mean_reply_time = ((mean_accumulator<PINGO_BLOCK_PING_TIME_NO_RESPONSE)?
                              mean_accumulator:PINGO_BLOCK_PING_TIME_NO_RESPONSE);
  }
  else
  {
    stats.mean_reply_time = PINGO_BLOCK_PING_TIME_NO_RESPONSE; 
    stats.min_reply_time  = PINGO_BLOCK_PING_TIME_NO_RESPONSE; 
    stats.max_reply_time  = PINGO_BLOCK_PING_TIME_NO_RESPONSE; 
  }

  return stats;
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

  if((file != nullptr) && (checksum != nullptr))
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

  if((directory != nullptr) && (filename != nullptr) && (path != nullptr) && (path_buffer_size > 0))
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

inline void fill_file_data(file_s *file, ping_block_c* ping_block)
{
  assert(file != nullptr);
  assert(ping_block != nullptr);

  /* Fill data */
  file->data = (file_data_entry_s*) calloc(sizeof(file_data_entry_s), file->header.address_count);
  for(uint_fast32_t i = 0; i < ping_block->get_address_count(); i++)
  {
    ping_block_entry_s ping_block_entry;
    assert(ping_block->get_ping_block_entry((ping_block->get_first_address()+i), &ping_block_entry));
    if(ping_block_entry.reply_valid)
    {
      file->data[i].type = FILE_DATA_ENTRY_ECHO_REPLY;
      file->data[i].payload.echo_reply.reply_time = 
        ((ping_block_entry.ping_time < FILE_ECHO_REPLY_TIME_MAX)?ping_block_entry.ping_time:FILE_ECHO_REPLY_TIME_MAX);
    }
    else if(ping_block_entry.skip_reason != PING_BLOCK_IP_SKIP_REASON_NOT_SKIPPED)
    {
      file->data[i].type = FILE_DATA_ENTRY_ECHO_SKIPPED;
      file->data[i].payload.echo_skipped.reason     = (file_data_entry_payload_echo_skip_reason_e) ping_block_entry.skip_reason;
      file->data[i].payload.echo_skipped.error_code = 
        ((((unsigned int) ping_block_entry.skip_errno) < FILE_ECHO_SKIPPED_ERROR_CODE_MAX)?ping_block_entry.skip_errno:FILE_ECHO_SKIPPED_ERROR_CODE_MAX);
    }
    else
    {
      file->data[i].type = FILE_DATA_ENTRY_ECHO_NO_REPLY;
      file->data[i].payload.echo_reply.reply_time = FILE_ECHO_REPLY_TIME_MAX;
    }
  }
}

inline bool write_file(const file_s *file, const char * path)
{
  bool ret_val = true;
  FILE * file_ptr;

  block_exit(EXIT_BLOCK_WRITE_FILE_OPEN);
  if((file_ptr = fopen(path, "wb")) != nullptr)
  {
    assert(1 == fwrite(&file->header,  sizeof(file->header), 1, file_ptr));
    assert(file->header.address_count == 
                fwrite(file->data,     sizeof(file_data_entry_s), file->header.address_count, file_ptr));
    assert(1 == fwrite(file->checksum, sizeof(file->checksum), 1, file_ptr));
    assert(0 == fclose(file_ptr));
  }
  else
  {
    fprintf(stderr, "Failed to open file '%s' for writing.  errno %u: %s\n", path, errno, strerror(errno));
    ret_val = false;
  }
  unblock_exit(EXIT_BLOCK_WRITE_FILE_OPEN);

  return ret_val;
}

bool file_manager_c::write_ping_block_to_file(ping_block_c* ping_block)
{
  bool   ret_val = true;
  char   ip_string_buffer[IP_STRING_SIZE];
  char   filename[FILE_NAME_MAX_LENGTH];
  char   path[FILE_PATH_MAX_LENGTH];
  file_s file;

  if((ping_block != nullptr) && (ping_block->get_address_count() > 0))
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
      fill_file_data(&file, ping_block);

     /* Fill checksum */
      generate_file_checksum(&file, file.checksum);

      /* Write file */
      write_file(&file, path);

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
      ping_block, ((ping_block != nullptr)?ping_block->get_address_count():-1));
    ret_val = false;
  }

  return ret_val;
}

void file_manager_c::sort_registry()
{
  registry_entry_s swap_buffer;
  unsigned int valid_entries = 0; 

  if( !registry.empty() &&
      FILE_REGISTRY_READ_AND_VALID(registry[0].state))
  {
    valid_entries++;
  }
  /* Insertion sort */
  for(unsigned int i = 1; i < registry.size(); i++)
  {
    if(FILE_REGISTRY_READ_AND_VALID(registry[i].state))
    {
      valid_entries++;
      if( (!FILE_REGISTRY_READ_AND_VALID(registry[i-1].state)) ||
          (registry[i].file.header.first_address < registry[i-1].file.header.first_address) )
      {
        swap_buffer = registry[i];
        unsigned int loop_j;
        for( loop_j = i; loop_j > 0; loop_j--)
        {
          if( (FILE_REGISTRY_READ_AND_VALID(registry[loop_j-1].state)) &&
              (swap_buffer.file.header.first_address > registry[loop_j-1].file.header.first_address) )
          {
            break;
          }

          registry[loop_j] = registry[loop_j-1];
        }
        registry[loop_j] = swap_buffer;
      }
    }
  }
  registry.resize(valid_entries);
}

bool file_manager_c::add_file_to_registry(const char * file_name, const file_s* file, registry_entry_state_e state)
{
  bool ret_val = true;
  registry_entry_s  registry_entry;

  if((file_name != nullptr) && (file != nullptr) && (state < FILE_REGISTRY_ENTRY_MAX))
  {
    memset(&registry_entry, 0, sizeof(registry_entry_s));
    strncpy(registry_entry.file_name, file_name, sizeof(registry_entry.file_name));
    registry_entry.file = *file;
    registry_entry.state = state;
    registry.push_back(registry_entry);
  }
  else
  {
    fprintf(stderr, "Bad input to add file to registry.  file_name %s file %p state %u\n", ((file_name != nullptr)?file_name:"(null)"), file, state);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::build_registry()
{
  bool              ret_val = true;
  char              file_path[FILE_PATH_MAX_LENGTH];
  char              ip_string_buffer[IP_STRING_SIZE];
  struct dirent    *dirent_ptr;
  DIR              *dir;
  file_s            file;

  dir = opendir(working_directory);

  if(dir != nullptr)
  {
    errno = 0;

    while((dirent_ptr = readdir(dir)) != nullptr)
    {
      file_path_from_directory_filename(working_directory, dirent_ptr->d_name, file_path, sizeof(file_path));

      if(read_file(file_path, &file, true) && file_header_valid(&file))
      {
        add_file_to_registry(dirent_ptr->d_name, &file, FILE_REGISTRY_ENTRY_READ_HEADER_ONLY);
        if(config.verbose)
        {
          ip_string(file.header.first_address, ip_string_buffer, sizeof(ip_string_buffer));
          printf("Found pingo file '%s' for ping block starting at IP %s with %u addresses\n", 
            dirent_ptr->d_name, ip_string_buffer, file.header.address_count);
        }
      }
      else
      {
        add_file_to_registry(dirent_ptr->d_name, &file, FILE_REGISTRY_ENTRY_INVALID_HEADER);
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
  std::vector<registry_entry_s>::iterator itr;

  sort_registry();

  for(itr = registry.begin(); itr != registry.end(); itr++)
  {
    if(FILE_REGISTRY_READ_AND_VALID(itr->state))
    {
      if( (ret_val >= itr->file.header.first_address) && 
          (ret_val < (itr->file.header.first_address + itr->file.header.address_count)) )
      {
        ret_val = (itr->file.header.first_address + itr->file.header.address_count);
      }
      else
      {
        break;
      }
    }
    else
    {
      fprintf(stderr, "File registry %s in unexpected state %u when getting next registry hole.", itr->file_name, itr->state);
    }
  }

  return ret_val;
}

bool file_manager_c::validate_files_in_registry()
{
  bool ret_val = true;
  bool valid_file_found = false;
  std::vector<registry_entry_s>::iterator itr;
  uint32_t last_file_last_ip  = -1;
  file_stats_s stats;
  char     file_path[FILE_PATH_MAX_LENGTH];
  char     ip_string_buffer_a[IP_STRING_SIZE];
  char     ip_string_buffer_b[IP_STRING_SIZE];

  sort_registry();

  for(itr = registry.begin(); itr != registry.end(); itr++)
  {
    if(FILE_REGISTRY_VALID_HEADER(itr->state))
    {
      file_path_from_directory_filename(working_directory, itr->file_name, file_path, sizeof(file_path));

      read_file(file_path, &itr->file);
      itr->state = (verify_checksum(&itr->file)?FILE_REGISTRY_ENTRY_READ_HEADER_ONLY_VALIDATED:FILE_REGISTRY_ENTRY_CORRUPTED);

      if(itr->file.header.first_address > (last_file_last_ip+1))
      {
        ip_string((last_file_last_ip+1),         ip_string_buffer_a, sizeof(ip_string_buffer_a));
        ip_string((itr->file.header.first_address-1), ip_string_buffer_b, sizeof(ip_string_buffer_b));
        printf("No data for IPs %s - %s\n", ip_string_buffer_a, ip_string_buffer_b);
        ret_val = false;
      }

      ip_string(itr->file.header.first_address, ip_string_buffer_a, sizeof(ip_string_buffer_a));
      ip_string(((itr->file.header.first_address+itr->file.header.address_count)-1), ip_string_buffer_b, sizeof(ip_string_buffer_b));
      if(FILE_REGISTRY_ENTRY_CORRUPTED == itr->state)
      {
        printf("CORRUPTED FILE '%s' FOR IPs %s - %s!\n", itr->file_name, ip_string_buffer_a, ip_string_buffer_b);
        ret_val = false;
      }
      else
      {
        if(config.stats_on_validation)
        {
          stats = get_stats_from_file(&itr->file);
          printf("File '%s' for IPs %s - %s validated. % 3d%% replied (count: %u, min: %u, mean: %u, max: %u skipped: %u)\n", 
            itr->file_name, ip_string_buffer_a, ip_string_buffer_b,
            (stats.valid_replies*PERCENT_100)/itr->file.header.address_count, 
            stats.valid_replies, stats.min_reply_time, stats.mean_reply_time, stats.max_reply_time, stats.echos_skipped);
        }
        else
        {
          printf("File '%s' for IPs %s - %s validated.\n", itr->file_name, ip_string_buffer_a, ip_string_buffer_b);
        }

        last_file_last_ip = (itr->file.header.first_address + itr->file.header.address_count)-1;
        valid_file_found = true;
      }

      delete_file_data(&itr->file);
    }
  }

  if( (last_file_last_ip < MAX_IP) ||
      (!valid_file_found) )
  {
    ip_string((last_file_last_ip+1),         ip_string_buffer_a, sizeof(ip_string_buffer_a));
    ip_string(MAX_IP, ip_string_buffer_b, sizeof(ip_string_buffer_b));
    printf("No data for IPs %s - %s\n", ip_string_buffer_a, ip_string_buffer_b);
    ret_val = false;
  }

  return ret_val;
}

bool file_manager_c::load_file_data(registry_entry_s* registry_entry)
{
  bool ret_val = true;

  if(registry_entry != nullptr)
  {
    char     file_path[FILE_PATH_MAX_LENGTH];
    file_path_from_directory_filename(working_directory, registry_entry->file_name, file_path, sizeof(file_path));

    if(read_file(file_path, &registry_entry->file))
    {
      if(verify_checksum(&registry_entry->file))
      {
        registry_entry->state = FILE_REGISTRY_ENTRY_READ_VALID;
      }
      else
      {
        registry_entry->state = FILE_REGISTRY_ENTRY_CORRUPTED;
        delete_file_data(&registry_entry->file);
      }
    }
    else
    {
      registry_entry->state = FILE_REGISTRY_ENTRY_INVALID_HEADER;
    }
  }

  return ret_val;
}

void file_manager_c::iterate_file_registry(file_iterator_cb callback, const void * user_data_ptr, uint32_t first_address, uint_fast64_t address_count)
{
  const uint_fast64_t last_address = first_address+address_count;

  if((callback != nullptr) && (last_address > first_address))
  {
    sort_registry();

    std::vector<registry_entry_s>::iterator itr;
    for(itr = registry.begin(); itr != registry.end(); itr++)
    {
      if(FILE_REGISTRY_VALID_HEADER(itr->state))
      {
        uint_fast64_t file_last_address = (itr->file.header.first_address + itr->file.header.address_count);
        if( (file_last_address > itr->file.header.first_address) &&
            (first_address < file_last_address) &&
            (itr->file.header.first_address < last_address) )
        {
          load_file_data(&(*itr));
          if(FILE_REGISTRY_ENTRY_READ_VALID == itr->state)
          {
            callback(&itr->file, user_data_ptr);
            itr->state = FILE_REGISTRY_ENTRY_READ_HEADER_ONLY_VALIDATED;
            delete_file_data(&itr->file);
          }
        }

        if(itr->file.header.first_address >= last_address)
        {
          break;
        }
      }
    }
  }
}