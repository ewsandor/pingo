#include <assert.h>

#include "ping_logger.hpp"

using namespace sandor_laboratories::pingo;

inline void ping_logger_c::lock_ping_block()
{
  assert(0 == pthread_mutex_lock(&ping_block_mutex));
}
inline void ping_logger_c::unlock_ping_block()
{
  assert(0 == pthread_mutex_unlock(&ping_block_mutex));
}

/* Pushes a ping block into the logger database.  Pusher's is responsible to init and dispatch pushed ping block */
void ping_logger_c::push_ping_block(ping_block_c* ping_block)
{
  lock_ping_block();

  ping_block_queue.push_back(ping_block);
  pthread_cond_broadcast(&ping_block_ready_cond);

  unlock_ping_block();
}

/* Pops the oldest ping block from the logger database. Popper's responsibility to delete popped ping block
    Returns null if no ping blocks are in the logger database. */
ping_block_c* ping_logger_c::pop_ping_block()
{
  ping_block_c* ret_ptr = nullptr;

  lock_ping_block();

  if(!ping_block_queue.empty())
  {
    ret_ptr = ping_block_queue.front();
    ping_block_queue.pop_front();
  }

  unlock_ping_block();

  return ret_ptr;
}

/* Returns a pointer to the oldest ping block in the logger database without popping.  Popper should NOT delete peeked ping block.
    Returns null if no ping blocks are in the logger database. */
ping_block_c* ping_logger_c::peek_ping_block()
{
  ping_block_c* ret_ptr = nullptr;

  lock_ping_block();

  if(!ping_block_queue.empty())
  {
    ret_ptr = ping_block_queue.front();
  }

  unlock_ping_block();

  return ret_ptr;
}

/* Blocks until ping block is added to the logger database */
void ping_logger_c::wait_for_ping_block()
{
  lock_ping_block();

  while(ping_block_queue.empty())
  {
    assert(0==pthread_cond_wait(&ping_block_ready_cond, &ping_block_mutex));
  }

  unlock_ping_block();
}

unsigned int ping_logger_c::get_num_ping_blocks()
{
  unsigned int ret_val = 0;

  lock_ping_block();

  ret_val = ping_block_queue.size();

  unlock_ping_block();
  
  return ret_val;
}