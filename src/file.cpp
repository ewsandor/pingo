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

bool file_manager_c::write_ping_block_to_file(ping_block_c* ping_block)
{
  bool   ret_val = true;
  uint_fast32_t i;
  char   ip_string_buffer[IP_STRING_SIZE];
  char   filename[FILE_PATH_MAX_LENGTH+FILE_NAME_MAX_LENGTH];
  file_s file;
  FILE * fp;
  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;

  if(ping_block && (ping_block->get_address_count() > 0))
  {
    ip_string(ping_block->get_first_address(), ip_string_buffer, sizeof(ip_string_buffer), '_', true);
    snprintf(filename, sizeof(filename), "%s/%s.pingo", working_directory, ip_string_buffer);

    memset(&file, 0, sizeof(file));

    /* Reset checksum context */
    EVP_DigestInit(mdctx, md);
    /* Build header */
    file.header.signature     = FILE_SIGNATURE;
    file.header.version       = FILE_VERSION_0;
    file.header.first_address = ping_block->get_first_address();
    file.header.address_count = ping_block->get_address_count();
    EVP_DigestUpdate(mdctx, &file.header, sizeof(file.header));
    /* Fill data */
    file.data = (file_data_entry_s*) calloc(sizeof(file_data_entry_s), file.header.address_count);
    for(i = 0; i < ping_block->get_address_count(); i++)
    {
    }
    EVP_DigestUpdate(mdctx, file.data, sizeof(file_data_entry_s)*file.header.address_count);
    /* Fill checksum */
    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    assert(sizeof(file.checksum) == md_len);
    memcpy(file.checksum, md_value, sizeof(file.checksum));

    block_exit(EXIT_BLOCK_WRITE_FILE_OPEN);
    if((fp = fopen(filename, "wb")))
    {
      assert(1                         == fwrite(&file.header, sizeof(file.header), 1, fp));
      assert(file.header.address_count == fwrite(file.data, sizeof(file_data_entry_s), file.header.address_count, fp));
      assert(1                         == fwrite(file.checksum, sizeof(file.checksum), 1, fp));
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