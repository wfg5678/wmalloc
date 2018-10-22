# wmalloc
An implementation of a memory allocator in C

  wmalloc works by using the mmap and sbrk commands to request memory
  from the OS.

  How it works:

  The basic unit of memory is called a "chunk". A chunk consists of
  memory that is available for the user who requested the memory.
  Generally, many chunks will sit side by side in memory. Each chunk 
  also has 24 bytes of overhead for tracking. The three bytes are for:


  -Holding the size of the chunk that precedes it memory (8 bytes)
  -Holding the size of the chunk itself (8 bytes)
  -Holding the size of the chunk that is next in memory (8 bytes)


  Chunks exist in two states: available and unavailable.
  Available chunks are held in bins by size and wait for a request for
  memory. Unavailable chunks are released to the user for use as he 
  pleases.

   Look at the layout of memory for available and unavailable chunks
   in wmalloc.h.

 
   Note the flags for the previous and next chunk sizes. These flags
   are the upper most bit of the 8 byte block. A flag set to 0 indi-
   cates that the next or previous chunk is available. A flag of 1
   indicates that the chunk is unavailable. This is useful when a
   chunk is released. If one of its neighboring chunks is also avail-
   able they can be consolidated into a larger chunk. This decreases
   fragmentation.


   The available chunks are held in a set of linked lists. The first 
   call to wmalloc creates a set of bins. These bins are divided into
   sizes. Each bin holds any available chunks in a certain range. When
   a chunk of memory is requested the proper bin is searched for a 
   chunk of suitable size.

   bin 0: up to 40 bytes: [dummy] -> [chunk1] -> [chunk2] -> NULL
   
   bin 1: up to 48 bytes: [dummy] -> [chunk3] ->  NULL
   
   bin 2: up to 56 bytes: [dummy] -> [chunk4] -> [chunk5] -> NULL

   ...

   bin 35: up to 1024 bytes: [dummy] -> [chunk6] -> [chunk7] -> NULL

   ...

   Note that each bin has a dummy chunk as the first chunk. This 
   allows for easy removal without having to hold a reference to the
   head of the linked list in the case of removing the first chunk.

---------------------------------------------------------------------

   For Use in C file:

            #include "wmalloc.h" 

   Compiling:

            $gcc -std=gnu99 example.c -o example

  Call wmalloc and wfree the same as if using malloc and free from the
  stdlib.h header

  For example:   int* array = wmalloc(sizeof(int)*1000);
                 wfree(array);

----------------------------------------------------------------------

  A reference that was found helpful in the design
  https://code.woboq.org/userspace/glibc/malloc/malloc.c.html

