#ifndef __PING_LOGGER_HPP__
#define __PING_LOGGER_HPP__

#include <deque>
#include <pthread.h>

#include "pingo.hpp"
#include "ping_block.hpp"

namespace sandor_laboratories
{
  namespace pingo
  {
    typedef enum
    {
      PING_LOG_ENTRY_INVALID,
      PING_LOG_ENTRY_ECHO_REPLY,
      PING_LOG_ENTRY_MAX,

    } ping_log_entry_type_e; 

    typedef struct 
    {
      ping_log_entry_type_e type;
    } ping_log_entry_header_s;

    typedef struct
    {
      struct timespec reply_delay;
      pingo_payload_t payload;
    } ping_logger_entry_echo_reply_s;

    typedef union
    {
      ping_logger_entry_echo_reply_s echo_reply;
    } ping_log_entry_data_u;

    typedef struct 
    {
      ping_log_entry_header_s header;
      ping_log_entry_data_u   data;
    } ping_log_entry_s;

    typedef std::deque<ping_log_entry_s> log_queue_t;
    typedef std::deque<ping_block_c*>    ping_block_queue_t;

    class ping_logger_c
    {
      private:
        pthread_mutex_t    log_entry_mutex       = PTHREAD_MUTEX_INITIALIZER;
        pthread_cond_t     log_entry_ready_cond  = PTHREAD_COND_INITIALIZER;
        log_queue_t        log_entry_queue;

        void lock_log_entry();
        void unlock_log_entry();
        /* Pops the oldest log entry from the log entry queue. Returns entry of type INVALID if queue is empty. */
        ping_log_entry_s pop_log_entry();

        void process_echo_reply_log_entry(ping_log_entry_s *);

        pthread_mutex_t    ping_block_mutex      = PTHREAD_MUTEX_INITIALIZER;
        pthread_cond_t     ping_block_ready_cond = PTHREAD_COND_INITIALIZER;
        ping_block_queue_t ping_block_queue;
        
        void lock_ping_block();
        void unlock_ping_block();

      public:
        /* Pushes a log entry into the log database */
        bool             push_log_entry(ping_log_entry_s);
        /* Blocks until log entry is added to the logger database */
        void             wait_for_log_entry();
        /* Process the next log entry in the queue */
        void             process_log_entry();

        /* Pushes a ping block into the logger database.  Pusher's is responsible to init and dispatch pushed ping block */
        bool          push_ping_block(ping_block_c*);
        /* Returns a pointer to the oldest ping block in the logger database without popping.  Popper should NOT delete peeked ping block.
            Returns null if no ping blocks are in the logger database. */
        ping_block_c* peek_ping_block();
        /* Pops the oldest ping block from the logger database. Popper's responsibility to delete popped ping block
            Returns null if no ping blocks are in the logger database. */
        ping_block_c* pop_ping_block();
        /* Blocks until ping block is added to the logger database */
        void          wait_for_ping_block();
        /* Returns the number or registered ping blocks */
        unsigned int  get_num_ping_blocks();
    };
  }
}

#endif /* __PING_LOGGER_HPP__ */
