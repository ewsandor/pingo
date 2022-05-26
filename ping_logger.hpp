#ifndef __PING_LOGGER_HPP__
#define __PING_LOGGER_HPP__

#include <deque>
#include <pthread.h>

#include "ping_block.hpp"

namespace sandor_laboratories
{
  namespace pingo
  {
    typedef enum
    {
      PING_LOGGER_ENTRY_INVALID,
      PING_LOGGER_ENTRY_ECHO_REPLY,
      PING_LOGGER_ENTRY_MAX,

    } ping_logger_entry_type_e; 

    typedef struct 
    {
      ping_logger_entry_type_e type;
    } ping_logger_entry_header_s;

    typedef struct
    {
    } ping_logger_entry_echo_reply_s;

    typedef union
    {
      ping_logger_entry_echo_reply_s echo_reply;
    } ping_logger_entry_data_u;

    typedef struct 
    {
      ping_logger_entry_header_s header;
    } ping_logger_entry_s;

    #define PING_LOGGER_MAX_NUM_PING_BLOCKS 5

    typedef std::deque<ping_logger_entry_s> log_queue_t;
    typedef std::deque<ping_block_c*>       ping_block_queue_t;

    class ping_logger_c
    {
      private:
        pthread_mutex_t    log_entry_mutex       = PTHREAD_MUTEX_INITIALIZER;
        pthread_cond_t     log_entry_ready_cond  = PTHREAD_COND_INITIALIZER;
        log_queue_t        log_entry_queue;

        pthread_mutex_t    ping_block_mutex      = PTHREAD_MUTEX_INITIALIZER;
        pthread_cond_t     ping_block_ready_cond = PTHREAD_COND_INITIALIZER;
        ping_block_queue_t ping_block_queue;
        
        void lock_ping_block();
        void unlock_ping_block();
      public:
        /* Pushes a ping block into the logger database.  Pusher's is responsible to init and dispatch pushed ping block */
        void          push_ping_block(ping_block_c*);
        /* Returns a pointer to the oldest ping block in the logger database without popping.  Popper should NOT delete peeked ping block.
            Returns null if no ping blocks are in the logger database. */
        ping_block_c* peek_ping_block();
        /* Pops the oldest ping block from the logger database. Popper's responsibility to delete popped ping block
            Returns null if no ping blocks are in the logger database. */
        ping_block_c* pop_ping_block();
        /* Blocks until ping block is added to the logger database */
        void          wait_for_ping_block();
    };
  }
}

#endif /* __PING_LOGGER_HPP__ */
