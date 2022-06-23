#include <cassert>
#include <cstdio>
#include <cstring>

#include "ping_logger.hpp"

using namespace sandor_laboratories::pingo;

inline void ping_logger_c::lock_log_entry()
{
  assert(0 == pthread_mutex_lock(&log_entry_mutex));
}
inline void ping_logger_c::unlock_log_entry()
{
  assert(0 == pthread_mutex_unlock(&log_entry_mutex));
}

inline void ping_logger_c::lock_ping_block()
{
  assert(0 == pthread_mutex_lock(&ping_block_mutex));
}
inline void ping_logger_c::unlock_ping_block()
{
  assert(0 == pthread_mutex_unlock(&ping_block_mutex));
}

/* Pushes a ping block into the logger database.  Pusher's is responsible to init and dispatch pushed ping block */
bool ping_logger_c::push_ping_block(ping_block_c* ping_block)
{
  bool ret_val = true;

  lock_ping_block();

  ping_block_queue.push_back(ping_block);
  pthread_cond_broadcast(&ping_block_ready_cond);

  unlock_ping_block();

  return ret_val;
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


bool ping_logger_c::push_log_entry(ping_log_entry_s log_entry)
{
  bool ret_val = true;

  lock_log_entry();

  log_entry_queue.push_back(log_entry);
  pthread_cond_broadcast(&log_entry_ready_cond);

  unlock_log_entry();

  return ret_val;
}

ping_log_entry_s ping_logger_c::pop_log_entry()
{
  ping_log_entry_s ret_val;

  memset(&ret_val, 0, sizeof(ret_val));

  lock_log_entry();

  if(!log_entry_queue.empty())
  {
    ret_val = log_entry_queue.front();
    log_entry_queue.pop_front();
  }

  unlock_log_entry();

  return ret_val;
}

void ping_logger_c::wait_for_log_entry()
{
  lock_log_entry();

  while(log_entry_queue.empty())
  {
    assert(0==pthread_cond_wait(&log_entry_ready_cond, &log_entry_mutex));
  }

  unlock_log_entry();
}

void ping_logger_c::process_echo_reply_log_entry(ping_log_entry_s *log_entry)
{
  bool late_reply = false;
  ping_block_queue_t::iterator it;
  uint_fast32_t reply_delay = PINGO_BLOCK_PING_TIME_NO_RESPONSE;

  if( (log_entry != nullptr) && 
      (PING_LOG_ENTRY_ECHO_REPLY==log_entry->header.type) && 
      (timespec_valid(&log_entry->data.echo_reply.reply_delay)))
  {
    reply_delay = (uint_fast32_t) TIMESPEC_TO_MS(log_entry->data.echo_reply.reply_delay);

    lock_ping_block();
    if(!ping_block_queue.empty())
    {
      it = ping_block_queue.begin();
      while(it != ping_block_queue.end())
      {
        if((*it)->log_ping_time(log_entry->data.echo_reply.payload.dest_address, reply_delay))
        {
          break;
        }
        it++;
      }
    }
    late_reply = (ping_block_queue.end() == it);
    unlock_ping_block();

    if(late_reply)
    {
      char ip_string_buffer[IP_STRING_SIZE];
      ip_string(log_entry->data.echo_reply.payload.dest_address, ip_string_buffer, sizeof(ip_string_buffer));
      fprintf(stderr, "Late echo reply, ping block already released.  Dest address %s, reply_delay %lu\n",
        ip_string_buffer, reply_delay);
    }
  }
  else
  {
    fprintf(stderr, "Invalid echo reply log entry\n");
  }
}

void ping_logger_c::process_log_entry()
{
  ping_log_entry_s log_entry = pop_log_entry();

  switch (log_entry.header.type)
  {
    case PING_LOG_ENTRY_ECHO_REPLY:
    {
      process_echo_reply_log_entry(&log_entry);
      break;
    }
    default:
    {
      fprintf(stderr, "Unexpected log entry type %u\n",  log_entry.header.type);
      break;
    }
  }
}