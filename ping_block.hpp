#ifndef __PING_BLOCK_HPP__
#define __PING_BLOCK_HPP__

#include <cstdint>
#include <pthread.h>
#include <time.h>

namespace sandor_laboratories
{
  namespace pingo
  {
    #define PINGO_BLOCK_PING_TIME_NO_RESPONSE 0xFF

    typedef struct __attribute__((packed))
    {
      /* Ping time in ms, -1 for no response */
      uint16_t ping_time;
    } ping_block_entry_s;

    class ping_block_c
    {
      private:
        const uint32_t      first_address;
        const unsigned int  address_count;
        ping_block_entry_s *entry;

        bool                fully_dispatched;
        struct timespec     dispatch_done_time;

        pthread_mutex_t     mutex;

      public:
        ping_block_c(uint32_t first_address, unsigned int address_count);
        ~ping_block_c();

        inline uint32_t get_first_address() const {return first_address;};
        inline uint32_t get_address_count() const {return address_count;};

        bool log_ping_time(uint32_t address, unsigned int ping_time);
    };
  }
}

#endif /* __PING_BLOCK_HPP__ */