#include <cassert>
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
  bool ret_val = true;

  if(ping_block)
  {

  }
  else
  {
    fprintf(stderr, "Null ping block passed for writing to file");
    ret_val = false;
  }

  return ret_val;
}