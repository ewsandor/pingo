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
        pthread_mutex_t    queue_mutex = PTHREAD_MUTEX_INITIALIZER;
        log_queue_t        log_queue;

        pthread_mutex_t    data_mutex = PTHREAD_MUTEX_INITIALIZER;
        ping_block_queue_t ping_block_queue;

      public:
    };
  }
}

#endif __PING_LOGGER_HPP__
