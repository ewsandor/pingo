#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include "file.hpp"
#include "pingo.hpp"

using namespace sandor_laboratories::pingo;

file_manager_c::file_manager_c(const char * wd)
{
  assert(wd);
  strncpy(working_directory, wd, sizeof(working_directory));
}

bool file_manager_c::write_ping_block_to_file(ping_block_c* ping_block)
{
  uint32_t i;
  bool   ret_val = true;
  char   ip_string_buffer[IP_STRING_SIZE];
  char   filename[FILE_PATH_MAX_LENGTH+FILE_NAME_MAX_LENGTH];
  file_s file;
  FILE * fp;

  if(ping_block && (ping_block->get_address_count() > 0))
  {
    ip_string(ping_block->get_first_address(), ip_string_buffer, sizeof(ip_string_buffer), '_', true);
    snprintf(filename, sizeof(filename), "%s/%s.pingo", working_directory, ip_string_buffer);

    block_exit(EXIT_BLOCK_WRITE_FILE_OPEN);
    fp = fopen(filename, "wb");

    if(fp)
    {
      memset(&file, 0, sizeof(file));

      file.header.signature     = FILE_SIGNATURE;
      file.header.version       = FILE_VERSION_0;
      file.header.first_address = ping_block->get_first_address();
      file.header.address_count = ping_block->get_address_count();
      fwrite(&file.header, sizeof(file.header), 1, fp);

      file.data = (file_data_entry_s*) calloc(sizeof(file_data_entry_s), file.header.address_count);
      for(i = 0; i < ping_block->get_address_count(); i++)
      {
      }
      fwrite(file.data, sizeof(file_data_entry_s), file.header.address_count, fp);

      fwrite(&file.checksum, sizeof(file.checksum), 1, fp);

      assert(0 == fclose(fp));
    }
    else
    {
      fprintf(stderr, "Failed to open file '%s' for writing.  errno %u: %s\n", filename, errno, strerror(errno));
      ret_val = false;
    }
    unblock_exit(EXIT_BLOCK_WRITE_FILE_OPEN);
  }
  else
  {
    fprintf(stderr, "Invalid ping block passed for writing to file.  pointer %p address_count %d\n", 
      ping_block, (ping_block?ping_block->get_address_count():-1));
    ret_val = false;
  }

  return ret_val;
}