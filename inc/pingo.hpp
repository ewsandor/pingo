#ifndef __PINGO_HPP__
#define __PINGO_HPP__

#include <cstdint>
#include <cstdlib>
#include <linux/limits.h>

namespace sandor_laboratories
{
  namespace pingo
  {
    #define ICMP_IDENTIFIER 0xEDED

    #define UNUSED(x) (void)(x)

    #define MAX(a,b) ((a>b)?a:b)
    #define MIN(a,b) ((a<b)?a:b)

    #define PERCENT_100 100

    #define BITS_8   8
    #define BITS_4   4
    #define BITS_2   2
    #define BITS_1   1

    #define MAX_IP 0xFFFFFFFF
    #define IP_BYTE_A_OFFSET 24
    #define IP_BYTE_B_OFFSET 16
    #define IP_BYTE_C_OFFSET  8
    #define IP_BYTE_D_OFFSET  0

    #define HILBERT_ORDER_FOR_32_BITS 16

    /* Maximum path length */
    #define FILE_NAME_MAX_LENGTH NAME_MAX
    #define FILE_PATH_MAX_LENGTH PATH_MAX

    typedef struct 
    {
      uint32_t        dest_address;
      struct timespec request_time;
    } pingo_payload_t;

    /* Time Utils */
    /* ms*1000us/ms*1000ns/us */
    #define MS_TO_NANOSEC(ms) (ms*1000UL*1000UL)
    #define MS_TO_SECONDS(ms) (ms/1000)
    #define SECONDS_TO_MS(s)  (s*1000UL)
    #define NANOSEC_TO_MS(ns) (ns/(1000UL*1000UL))
    #define TIMESPEC_TO_MS(timespec_in) ( SECONDS_TO_MS(timespec_in.tv_sec) + \
                                          NANOSEC_TO_MS(timespec_in.tv_nsec) )
    #define MS_TO_TIMESPEC(ms, timespec_out) { timespec_out.tv_sec = MS_TO_SECONDS(ms); timespec_out.tv_nsec = (MS_TO_NANOSEC(ms) % (1000UL*1000UL*1000UL)); }

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
    #define EXIT_STATUS_NO_PERMISSION 126
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