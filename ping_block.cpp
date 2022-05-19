#include <cassert>
#include <cstdlib>

#include "ping_block.hpp"

using namespace sandor_laboratories::pingo;

ping_block_c::ping_block_c(uint32_t first_address, unsigned int address_count)
  : first_address(first_address), address_count(address_count)
{
  unsigned int i;

  assert(0 == pthread_mutex_init(&mutex, NULL));

  assert(0 == pthread_mutex_lock(&mutex));

  fully_dispatched = false;
  dispatch_done_time = {0};

  entry = (ping_block_entry_s*) calloc(address_count, sizeof(ping_block_entry_s));

  for(i = 0; i < address_count; i++)
  {
    entry[i].ping_time = PINGO_BLOCK_PING_TIME_NO_RESPONSE;
  }

  assert(0 == pthread_mutex_unlock(&mutex));
}

ping_block_c::~ping_block_c()
{
  assert(0 == pthread_mutex_lock(&mutex));

  free(entry);

  assert(0 == pthread_mutex_unlock(&mutex));

  assert(0 == pthread_mutex_destroy(&mutex));
}


bool ping_block_c::log_ping_time(uint32_t address, unsigned int ping_time)
{
  bool ret_val = false;
  ping_block_entry_s* log_entry = nullptr;

  if((address-get_first_address()) < get_address_count())
  {
    ret_val = true;

    assert(0 == pthread_mutex_lock(&mutex));

    log_entry = &entry[(address-get_first_address())];
    
    log_entry->ping_time = 
      (ping_time<PINGO_BLOCK_PING_TIME_NO_RESPONSE)?
       ping_time:PINGO_BLOCK_PING_TIME_NO_RESPONSE;

    assert(0 == pthread_mutex_unlock(&mutex));
  }

  return ret_val;
}