#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>

#define ORIG_BUFFER "Arthur e Gustavo"

// The page frame shifted left by PAGE_SHIFT will give us the physcial address of the frame
// Note that this number is architecture dependent. For me on x86_64 with 4096 page sizes,
// it is defined as 12. If you're running something different, check the kernel source
// for what it is defined as.
#define PAGE_SHIFT 12
#define PAGEMAP_LENGTH 8

void* create_buffer(void);
unsigned long get_page_frame_number_of_address(void *addr);

int main(void) {

   // Cria o buffer e armazena na variável
   void *buffer = create_buffer();
   printf ("Buffer: %s\n", buffer);

   // Mostra o endereço lógico do buffer
   printf("Endereço logico do buffer: %p\n\n", (unsigned long)buffer);


   // Get the page frame the buffer is on
   unsigned int page_frame_number = get_page_frame_number_of_address(buffer);
   printf("Page frame: 0x%x\n", page_frame_number);

   // Find the difference from the buffer to the page boundary
   unsigned int distance_from_page_boundary = (unsigned long)buffer % getpagesize();

   // Determine how far to seek into memory to find the buffer
   uint64_t physc = (page_frame_number << PAGE_SHIFT) + distance_from_page_boundary;

   printf("Endereço físico: 0x%x\n", physc);

   free(buffer);

   return 0;
}

void* create_buffer(void) {
   size_t buf_size = strlen(ORIG_BUFFER) + 1;

   // Allocate some memory to manipulate
   void *buffer = malloc(buf_size);
   if(buffer == NULL) {
      fprintf(stderr, "Failed to allocate memory for buffer\n");
      exit(1);
   }

   // Lock the page in memory
   // Do this before writing data to the buffer so that any copy-on-write
   // mechanisms will give us our own page locked in memory
   if(mlock(buffer, buf_size) == -1) {
      fprintf(stderr, "Failed to lock page in memory: %s\n", strerror(errno));
      exit(1);
   }

   // Add some data to the memory
   strncpy(buffer, ORIG_BUFFER, strlen(ORIG_BUFFER));

   return buffer;
}

unsigned long get_page_frame_number_of_address(void *addr) {
   // Open the pagemap file for the current process
   FILE *pagemap = fopen("/proc/self/pagemap", "rb");

   // Seek to the page that the buffer is on
   unsigned long physc = (unsigned long)addr / getpagesize() * PAGEMAP_LENGTH;
   if(fseek(pagemap, (unsigned long)physc, SEEK_SET) != 0) {
      fprintf(stderr, "Failed to seek pagemap to proper location\n");
      exit(1);
   }

   // The page frame number is in bits 0-54 so read the first 7 bytes and clear the 55th bit
   unsigned long page_frame_number = 0;
   fread(&page_frame_number, 1, PAGEMAP_LENGTH-1, pagemap);
   
   page_frame_number &= 0x7FFFFFFFFFFFFF;

   fclose(pagemap);

   return page_frame_number;
}
