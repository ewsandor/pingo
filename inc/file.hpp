#ifndef __FILE_HPP__
#define __FILE_HPP__

#include <cstdint>
#include <openssl/evp.h>
#include <vector>

#include "ping_block.hpp"
#include "pingo.hpp"

namespace sandor_laboratories
{
  namespace pingo
  {
    /* File signature "PINGO" to identify Pingo file in little endian */
    #define FILE_SIGNATURE 0x4F474E4950
    /* MD5 checksums are 128bits (16 bytes) */
    #define MD5_SIZE 16
    /* Checksum is MD5 */
    #define FILE_CHECKSUM_SIZE MD5_SIZE

    typedef enum
    {
      FILE_VERSION_INVALID,
      FILE_VERSION_0,
      FILE_VERSION_MAX,
    } file_version_e;

    /* Header for Pingo file */
    typedef struct __attribute__ ((packed))
    {
      /* Static string "PINGO" if valid Pingo file*/
      uint64_t       signature:40;
      /* Version of Pingo file */
      file_version_e version:24;
      
      /* First IPv4 address in this file */
      uint32_t       first_address;
      /* Number of consecutive IPv4 addresses recorded in this file */
      uint32_t       address_count;

    } file_header_s;

    /* Types of data entries */
    typedef enum
    {
      FILE_DATA_ENTRY_INVALID,
      FILE_DATA_ENTRY_ECHO_REPLY,
      FILE_DATA_ENTRY_ECHO_NO_REPLY,
      FILE_DATA_ENTRY_ECHO_SKIPPED,
      FILE_DATA_ENTRY_MAX,
    } file_data_entry_type_e;

    #define FILE_ECHO_REPLY_TIME_MAX 0x00FFFFFF
    typedef struct __attribute__ ((packed))
    {
      /* Echo reply time in ms */
      uint32_t reply_time:24;
    } file_data_entry_payload_echo_reply_s;
    
    typedef enum 
    {
      FILE_DATA_ENTRY_ECHO_SKIP_REASON_NOT_SKIPPED,
      FILE_DATA_ENTRY_ECHO_SKIP_REASON_EXCLUDE_LIST,
      FILE_DATA_ENTRY_ECHO_SKIP_REASON_SOCKET_ERROR,
      FILE_DATA_ENTRY_ECHO_SKIP_REASON_MAX,
    } file_data_entry_payload_echo_skip_reason_e;
    #define FILE_ECHO_SKIPPED_ERROR_CODE_MAX 0x000FFFFF
    typedef struct __attribute__ ((packed))
    {
      /* Reason for skip */
      file_data_entry_payload_echo_skip_reason_e reason:4;
      /* Error code */
      uint32_t error_code:20;

    } file_data_entry_payload_echo_skipped_s;

    typedef union __attribute__ ((packed))
    {
      /* Data recording successful echo reply */
      file_data_entry_payload_echo_reply_s   echo_reply;
      /* Echo skipped for data entry */
      file_data_entry_payload_echo_skipped_s echo_skipped;
    } file_data_entry_payload_u;

    typedef struct __attribute__ ((packed))
    { 
      /* Identifies type of data entry */
      file_data_entry_type_e    type:8;
      /* Payload for data entry - 24 bits*/
      file_data_entry_payload_u payload;
    } file_data_entry_s;

    typedef uint8_t file_checksum_t[FILE_CHECKSUM_SIZE];

    typedef struct __attribute__ ((packed))
    {
      /* Header details */
      file_header_s      header;
      /* Array of data entries.  Entries equals header address_count */
      file_data_entry_s *data;
      /* Checksum.  Assumed to be 0 for calculation */
      file_checksum_t    checksum;
    } file_s;

    #define FILE_REGISTRY_READ_AND_VALID(state) ((state > FILE_REGISTRY_ENTRY_UNREAD) && (state < FILE_REGISTRY_ENTRY_CORRUPTED))
    #define FILE_REGISTRY_VALID_HEADER(state)   ((state != FILE_REGISTRY_ENTRY_UNREAD) && (state != FILE_REGISTRY_ENTRY_INVALID_HEADER))
    typedef enum
    {
      FILE_REGISTRY_ENTRY_UNREAD,
      FILE_REGISTRY_ENTRY_READ_HEADER_ONLY,
      FILE_REGISTRY_ENTRY_READ_HEADER_ONLY_VALIDATED,
      FILE_REGISTRY_ENTRY_READ_NOT_VALIDATED,
      FILE_REGISTRY_ENTRY_READ_VALID,
      FILE_REGISTRY_ENTRY_CORRUPTED,
      FILE_REGISTRY_ENTRY_INVALID_HEADER,
      FILE_REGISTRY_ENTRY_MAX,
    } registry_entry_state_e;

    typedef struct 
    {
      registry_entry_state_e state;
      char                   file_name[FILE_NAME_MAX_LENGTH];
      file_s                 file;

    } registry_entry_s;

    typedef struct
    {
      bool verbose;
      bool stats_on_validation;
    } file_manager_config_s;

    typedef struct 
    {
      unsigned int valid_replies;
      unsigned int echos_skipped;
      reply_time_t min_reply_time;
      reply_time_t mean_reply_time;
      reply_time_t max_reply_time;
    } file_stats_s;

    typedef void (*file_iterator_cb)(const file_s*, const void *);

    class file_manager_c
    {
      private:
        const file_manager_config_s    config;

        char                           working_directory[FILE_PATH_MAX_LENGTH];
        EVP_MD_CTX                    *mdctx;
        std::vector<registry_entry_s>  registry;
        
        static bool read_remaining_file   (const char * file_path, FILE * file_ptr, file_s* output_file, bool skip_data);
        static bool file_header_valid     (const file_s*);
        static bool file_data_valid       (const file_s*);
        static bool read_file_header      (FILE *, file_s*);
        static bool read_file_data        (FILE *, file_s*);
        static bool read_file_checksum    (FILE *, file_s*);
        static bool read_file             (const char *, file_s*, bool skip_data = false);
        static bool delete_file_data      (file_s*);
        static file_stats_s get_stats_from_file(const file_s*);

        static bool verify_checksum       (const file_s*, EVP_MD_CTX *);
        bool verify_checksum              (const file_s*);
        static bool generate_file_checksum(const file_s*, file_checksum_t, EVP_MD_CTX *);
        bool generate_file_checksum       (const file_s*, file_checksum_t);

        static bool file_path_from_directory_filename(const char * directory, const char * filename, char * path, size_t path_buffer_size);

        bool add_file_to_registry(const char *, const file_s*, registry_entry_state_e);
        void sort_registry       ();
        bool load_file_data      (registry_entry_s*);

      public:
        file_manager_c(const char * working_directory_);
        ~file_manager_c();

        bool build_registry();
        bool validate_files_in_registry();
        uint32_t get_next_registry_hole_ip();
        void iterate_file_registry(file_iterator_cb callback, const void * user_data_ptr, uint32_t first_address = 0, uint_fast64_t address_count = (1L<<32));

        bool write_ping_block_to_file(ping_block_c*);
    };
  }
}
 
#endif /* __FILE_HPP__ */