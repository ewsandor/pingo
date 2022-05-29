#ifndef __PING_BLOCK_HPP__
#define __PING_BLOCK_HPP__

#include <cstdint>
#include <pthread.h>
#include <time.h>

namespace sandor_laboratories
{
  namespace pingo
  {

    typedef uint32_t reply_time_t;
    #define PINGO_BLOCK_PING_TIME_NO_RESPONSE 0xFFFFFFFF

    typedef struct
    {
      bool reply_valid;
      /* Ping time in ms, -1 for no response */
      reply_time_t ping_time;
    } ping_block_entry_s;

    typedef struct 
    {
      bool            verbose;
      unsigned int    ping_batch_size;
      struct timespec ping_batch_cooldown;
      unsigned int    socket_ttl;
      uint16_t        identifier;
      unsigned int    send_attempts;
    } ping_block_config_s;

    typedef struct 
    {
      unsigned int valid_replies;
      reply_time_t min_reply_time;
      reply_time_t mean_reply_time;
      reply_time_t max_reply_time;
    } ping_block_stats_s;

    class ping_block_c
    {
      private:
        const uint32_t             first_address;
        const unsigned int         address_count;
        const ping_block_config_s  config;
        ping_block_entry_s        *entry;

        pthread_cond_t             dispatch_done_cond = PTHREAD_COND_INITIALIZER;
        bool                       dispatch_started;
        bool                       fully_dispatched;
        struct timespec            dispatch_start_time;
        struct timespec            dispatch_done_time;
        struct timespec            dispatch_time;

        pthread_mutex_t            mutex = PTHREAD_MUTEX_INITIALIZER;
        void                       lock();
        void                       unlock();

      public:
        static void init_config(ping_block_config_s*);

        ping_block_c(uint32_t first_address, unsigned int address_count);
        ping_block_c(uint32_t first_address, unsigned int address_count, const ping_block_config_s *);
        ~ping_block_c();

        inline uint32_t get_first_address() const {return first_address;};
        inline uint32_t get_address_count() const {return address_count;};
        inline uint32_t get_last_address()  const {return (get_first_address()+get_address_count());};

        /* Logs ping time.  Assumes ping reply is valid if called, but time may will be capped at PINGO_BLOCK_PING_TIME_NO_RESPONSE */
        bool log_ping_time(uint32_t address, reply_time_t);

        /* Opens a IPv4 socket and dispatches ping echo requests for all IP address in this block */
        bool dispatch();

        /* Returns true if ping block has started dispatching */
        bool            is_dispatch_started();
        /* Returns true if ping block has been fully dispatched */
        bool            is_fully_dispatched();
        /* Time this ping block started dispatching.  Returns 0 if not started dispatched */
        struct timespec get_dispatch_start_time();
        /* Time this ping block finished dispatching.  Returns 0 if not fully dispatched */
        struct timespec get_dispatch_done_time();
        /* Returns time it took to dispatch block.  Returns 0 if not fully dispatched */
        struct timespec get_dispatch_time();
        /* Time since fully dispatching this ping block.  Returns 0 if not fully dispatched */
        struct timespec time_since_dispatch();
        /* Blocks until dispatching is done */
        void            wait_dispatch_done();

        /* Returns stats from ping block data */
        ping_block_stats_s get_stats();
    };
  }
}

#endif /* __PING_BLOCK_HPP__ */