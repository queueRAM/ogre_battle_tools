#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>

#include "utils.h"

#define OUTPUT_DIR "ogre_dump"

// common LHA header
struct lha_header
{
   uint16_t header_length;
   char method[5];
   uint32_t compressed_size;
   uint32_t uncompressed_size;
   uint32_t date;
   uint8_t file_attribute;
   uint8_t level_identifier;
} __attribute__((packed));

// LHA level 1
struct level1_header
{
   struct lha_header lha;
   uint8_t filename_length;
} __attribute__((packed));

// LHA level 2
struct level2_header
{
   struct lha_header lha;
   uint16_t uncompressed_crc;
   uint8_t os_id;
   uint16_t next_header_size;
} __attribute__((packed));

#define LHA_EXE "lha"
void extract_lha(const char *dir, const char *file)
{
   char command[FILENAME_MAX];
   sprintf(command, "%s x -w %s %s", LHA_EXE, dir, file);
   int status = system(command);
   if (status) {
      printf("Error %d running %s\n", status, command);
   }
}

int dump_lha(const uint8_t *data, int offset)
{
   char out_path[FILENAME_MAX];
   char extract_dir[FILENAME_MAX];
   struct lha_header *hdr = (struct lha_header*)&data[offset];
   char method[6];
   char date_str[128];
   time_t tt;

   // confirm level
   printf("%08X: ", offset);
   if (hdr->level_identifier == 2 || hdr->level_identifier == 1) {
      printf("%04X ", hdr->header_length);
      memcpy(method, hdr->method, sizeof(hdr->method));
      method[sizeof(hdr->method)] = '\0';
      printf("%s ", method);
      printf("%d ", hdr->level_identifier);
      printf("%08X ", hdr->compressed_size);
      printf("%08X ", hdr->uncompressed_size);
      tt = hdr->date;
      struct tm *utc_time = gmtime(&tt);
      strftime(date_str, sizeof(date_str), "%F %T %z", utc_time);
      printf("%s ", date_str);

      switch (hdr->level_identifier) {
         case 1:
         {
            struct level1_header *l1hdr = (struct level1_header*)hdr;
            printf("%02X\n", l1hdr->filename_length);
            break;
         }
            break;
         case 2:
         {
            struct level2_header *l2hdr = (struct level2_header*)hdr;
            printf("%c\n", l2hdr->os_id);
            break;
         }
      }
      // output LHA
      sprintf(out_path, "%s/%08X.lha", OUTPUT_DIR, offset);
      write_file(out_path, &data[offset], hdr->compressed_size + hdr->header_length);

      // extract LHA
      sprintf(extract_dir, "%s/%08X", OUTPUT_DIR, offset);
      make_dir(extract_dir);
      extract_lha(extract_dir, out_path);
   } else {
      printf("Warning: unknown level identifier: %02X\n", hdr->level_identifier);
      return 0;
   }
   return 1;
}

// finds a binary pattern in memory, sort of like strstr() for non-string buffers
int find_bytes(const uint8_t *data, size_t data_len, const uint8_t *needle, size_t needle_len, size_t start)
{
   for (size_t i = start; i < data_len - needle_len; i++) {
      if (!memcmp(&data[i], &needle[0], needle_len)) {
         return i;
      }
   }
   return -1;
}

// find all LHA headers and dump blocks
void dump_all_lha(const uint8_t *data, size_t data_len)
{
   const char lha_magic[] = "-lh";
   const size_t magic_len = strlen(lha_magic);
   size_t start = 0;
   int found;
   int counts[8] = {0};
   int total_count = 0;
   do {
      found = find_bytes(data, data_len, (uint8_t*)lha_magic, magic_len, start);
      start = found + 1;
      if (found >= 0) {
         // verify valid header
         const unsigned type = data[found+3] - '0';
         if (type < DIM(counts) && data[found+4] == '-') {
            if (dump_lha(data, found-2)) {
               counts[type]++;
            }
         }
      }
   } while (found >= 0);

   for (unsigned i = 0; i < DIM(counts); i++) {
      printf("LH%u: %d\n", i, counts[i]);
      total_count += counts[i];
   }
   printf("Total: %d\n", total_count);
}

int main(int argc, char *argv[])
{
   char *filename;
   size_t length;
   uint8_t *data;

   if (argc < 2) {
      printf("Usage: ogre_split <ROM>\n");
      exit(1);
   }

   filename = argv[1];
   length = read_file(filename, &data);

   if (length > 0) {
      make_dir(OUTPUT_DIR);
      dump_all_lha(data, length);
      free(data);
   } else {
      printf("Error reading file \"%s\"", filename);
   }
   return 0;
}
