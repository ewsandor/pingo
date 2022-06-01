#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <openssl/evp.h>

#include "file.hpp"
#include "pingo.hpp"

using namespace sandor_laboratories::pingo;

file_manager_c::file_manager_c(const char * wd)
  : md(EVP_md5())
{
  assert(wd);
  strncpy(working_directory, wd, sizeof(working_directory));
  mdctx = EVP_MD_CTX_new();
}

file_manager_c::~file_manager_c()
{
  EVP_MD_CTX_free(mdctx);
}

bool file_manager_c::generate_file_checksum(const file_s *file, file_checksum_t checksum)
{
  bool ret_val = true;
  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;

  if(file && checksum)
  {
    /* Reset checksum context */
    EVP_DigestInit_ex(mdctx, md, nullptr);
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

bool file_manager_c::write_ping_block_to_file(ping_block_c* ping_block)
{
  bool   ret_val = true;
  uint_fast32_t i;
  char   ip_string_buffer[IP_STRING_SIZE];
  char   filename[FILE_PATH_MAX_LENGTH+FILE_NAME_MAX_LENGTH];
  file_s file;
  FILE * fp;
  ping_block_entry_s ping_block_entry;

  if(ping_block && (ping_block->get_address_count() > 0))
  {
    ip_string(ping_block->get_first_address(), ip_string_buffer, sizeof(ip_string_buffer), '_', true);
    snprintf(filename, sizeof(filename), "%s/%s.pingo", working_directory, ip_string_buffer);

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
    if((fp = fopen(filename, "wb")))
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
    fprintf(stderr, "Invalid ping block passed for writing to file.  pointer %p address_count %d\n", 
      ping_block, (ping_block?ping_block->get_address_count():-1));
    ret_val = false;
  }

  return ret_val;
}