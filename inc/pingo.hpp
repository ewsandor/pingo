#ifndef __PINGO_HPP__
#define __PINGO_HPP__

#include <cstdint>
#include <cstdlib>

#define ICMP_IDENTIFIER 0xEDED

namespace sandor_laboratories
{
  namespace pingo
  {
    #define UNUSED(x) (void)(x)

    typedef struct 
    {
      uint32_t        dest_address;
      struct timespec request_time;
    } pingo_payload_t;

    /* Time Utils */
    /* ms*1000us/ms*1000ns/us */
    #define MS_TO_NANOSEC(ms) (ms*1000*1000)
    #define SECONDS_TO_MS(s)  (s*1000)
    #define NANOSEC_TO_MS(ns) (ns/(1000*1000))
    #define TIMESPEC_TO_MS(timespec_in) ( SECONDS_TO_MS(timespec_in.tv_sec) + \
                                          NANOSEC_TO_MS(timespec_in.tv_nsec) )


    /* a-b=diff, returns false if input is null or b > a */
    bool diff_timespec(const struct timespec * a, const struct timespec * b, struct timespec * diff);
    bool timespec_valid(const struct timespec *);

    inline int get_time(struct timespec * out_timespec)
    {
      return clock_gettime(CLOCK_MONOTONIC_COARSE, out_timespec);
    }

    /* String Utils */
    #define IP_STRING_SIZE 16
    void ip_string(uint32_t address, char* buffer, size_t buffer_size, char deliminator='.', bool leading_zero=false);

    /* Exit Utils */
    typedef enum
    {
      EXIT_BLOCK_WRITE_FILE_OPEN,
      EXIT_BLOCK_INVALID,
    } exit_block_reason_e;
    void safe_exit(int status);
    void block_exit(exit_block_reason_e);
    void unblock_exit(exit_block_reason_e);

  }
}
 
#endif /* __PINGO_HPP__ */