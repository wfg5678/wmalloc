#ifndef WMALLOC
#define WMALLOC


#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

//#define NDEBUG

#include <assert.h>


/*
  wmalloc:

  My implementation of memory allocator written for POSIX Systems.

  wmalloc works by using the mmap and sbrk commands to request memory
  from the OS.

  How it works:

  The basic chunk of memory is called a "chunk". A chunk consists of
  memory that is available for the user who requested the memory.
  Generally, many chunks will sit side by side in memory. Each chunk 
  has 3 bytes of overhead for tracking. The three bytes are for:


  -Holding the size of the chunk that precedes it memory
  -Holding the size of the chunk itself
  -Holding the size of the chunk that is next in memory


  Chunks exist in two states: available and unavailable.
  Available chunks are held in bins by size and wait for a request for
  memory. Unavailable chunks are released to the user for use as he 
  pleases.

   Layout of memory for a chunk that has been given to the user

   +++++++++++++++++++++++
   + size of prev chunk  +
   + in use flag (1 bit) +
   +++++++++++++++++++++++
   + size of curr chunk  +
   +                     +
   +++++++++++++++++++++++ <---- mem address returned to user
   +                     +
   +      for user       +
   +                     +
   +++++++++++++++++++++++
   + size of next chunk  +
   + in use flag (1 bit) +
   +++++++++++++++++++++++



   Layout of memory for a chunk stored in a bin

   +++++++++++++++++++++++
   + size of prev chunk  +
   + in use flag (1 bit) +
   +++++++++++++++++++++++
   + size of curr chunk  +
   +                     +
   +++++++++++++++++++++++
   +  ptr to prev chunk  +
   +    <------          +
   +++++++++++++++++++++++
   +  ptr to next chunk  +
   +          ------->   +
   +++++++++++++++++++++++
   +                     +
   +    unused space     +
   +    (could be 0)     +
   +                     +
   +++++++++++++++++++++++
   + size of next chunk  +
   + in use flag (1 bit) +
   +++++++++++++++++++++++


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

   bin 0: up to 40 bytes: [dummy] -> [chunk1] -> [chunk2] -> x
   bin 1: up to 48 bytes: [dummy] -> [chunk3] ->  x
   bin 2: up to 56 bytes: [dummy] -> [chunk4] -> [chunk5] -> x

   ...

   bin 35: up to 1024 bytes: [dummy] -> [chunk5] -> [chunk6] -> x

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
*/


#define CHUNK_OVERHEAD 24
#define NUM_BINS 46

//the mimimum size that MMAP will request (32 pages of 4096 bytes)
#define MMAP_SIZE 0x20000

//Page Size
#define PAGE_SIZE 0x1000

#define MINIMUM_CHUNK_SIZE 40

struct chunk{

  uint64_t prev_chunk_size;
  uint64_t curr_chunk_size;
  struct chunk* left_ptr;
  struct chunk* right_ptr;
};



//the struct that holds the info for wmalloc
struct wmalloc_info{

  struct chunk* bin[NUM_BINS];
  struct chunk dummy[NUM_BINS];
  uint64_t bin_index[NUM_BINS];
};

//the pointer to the struct that holds the available chunks
struct wmalloc_info* wmalloc_ptr = NULL;
  

//--------------Initializing Functions-------------------------------

int initialize_wmalloc();
void initialize_bin_indices();


//--------------General Purpose Functions----------------------------

int is_prev_available(struct chunk* ch);
int is_next_available(struct chunk* ch);
void set_prev_chunk_size(struct chunk* ch, uint64_t prev_chunk_size);
void set_next_chunk_size(struct chunk* ch, uint64_t next_chunk_size);
uint64_t get_prev_chunk_size(struct chunk* ch);
uint64_t get_next_chunk_size(struct chunk* ch);
struct chunk* get_prev_chunk(struct chunk* ch);
struct chunk* get_next_chunk(struct chunk* ch);
void set_prev_flag(struct chunk* ch, int flag);
void set_next_flag(struct chunk* ch, int flag);
void set_unavailable(struct chunk* ch);
void set_available(struct chunk* ch);
void set_adjacent_sizes(struct chunk* ch, int available);

void add_chunk(struct chunk* to_add);
void insert_in_place(struct chunk* head, struct chunk* to_add);
int find_bin(uint64_t request_length);
struct chunk* search_bin(int i, int request_length);
struct chunk* check_bigger_bins(int i);

//-------------Allocating Functions----------------------------------


void* wmalloc(uint64_t request_length);
struct chunk* allocate_chunk(uint64_t request_length);
void split_chunk(struct chunk* to_remove, uint64_t request_length);
struct chunk* remove_chunk(struct chunk* to_remove);

//------------Freeing Functions--------------------------------------

void wfree(void* to_free);
struct chunk* join_chunks(struct chunk* first, struct chunk* second);




/*
  Uses sbrk to move the program break and allocate memory for the
  struct that holds the pointers to the bins of chunks

  Return -1 if sbrk fails to allocate requested memory

  For some reason moves sbrk more than requested... Wonder why?
  Seems to be pattern for first call to sbrk() by a program.
  This is not a problem as long as it always gives at least what is 
  requested.
*/
int initialize_wmalloc(){

  wmalloc_ptr = sbrk(sizeof(struct wmalloc_info));

  //check allocation
  if(wmalloc_ptr == (void*) -1){
    return -1;
  }

  //each bin begins with a dummy node with chunk_size 0
  for(int i=0; i<NUM_BINS; i++){
    
    wmalloc_ptr->dummy[i].curr_chunk_size = 0;
    wmalloc_ptr->dummy[i].left_ptr = NULL;
    wmalloc_ptr->dummy[i].right_ptr = NULL;
    
    wmalloc_ptr->bin[i] = &wmalloc_ptr->dummy[i];
  }

  initialize_bin_indices();
  
  return 1;
}

/*
  sets the indices for the bins
  bins are sized:
  
  40 to 128 bytes by 8
  144 to 256 bytes by 16
  288 to 515 bytes by 32
  576 to 1024 bytes by 64
  2048 to 524288 by multiple of 2
  1 bin for anything larger than 524288
*/ 

void initialize_bin_indices(){

  assert(wmalloc_ptr != NULL);
  
  int index=0;
  for(int i=40; i<=128; i=i+8){
    wmalloc_ptr->bin_index[index] = i;
    index++;
  }
  for(int i=144; i<=256; i=i+16){
    wmalloc_ptr->bin_index[index] = i;
    index++;
  }
  for(int i=288; i<=512; i=i+32){
    wmalloc_ptr->bin_index[index] = i;
    index++;
  }
  for(int i=576; i<=1024; i=i+64){
    wmalloc_ptr->bin_index[index] = i;
    index++;
  }
  for(int i=2048; i<1000000; i= i*2){
    wmalloc_ptr->bin_index[index] = i;
    index++;
  }

  //set max bin limit
  wmalloc_ptr->bin_index[index] = 0xffffffffffffffff;

  return;  
}

/*
  Examines previous chunk. Returns 1 if prev chunk is 
  available to be joined. If previous chunk is in use
  function will return 0
*/
int is_prev_available(struct chunk* ch){

  assert(ch != NULL);
  
  uint64_t prev_chunk_size = ch->prev_chunk_size;

  if(prev_chunk_size != 0){
    prev_chunk_size = prev_chunk_size >> 63;

    if(prev_chunk_size == 0){
      return 1;
    }
  }
  return 0;
}

/*
  Examines next chunk. Returns 1 if next chunk is 
  available to be joined. If next chunk is in use
  function will return 0.
*/
int is_next_available(struct chunk* ch){

  assert(ch != NULL);

  uint64_t address = (uint64_t) ch;
  address = address + ch->curr_chunk_size - 8;
  uint64_t* ptr_to_size = (uint64_t*) address;
  
  if((*ptr_to_size) != 0){

    uint64_t next_chunk_size  = (*ptr_to_size);
    next_chunk_size = next_chunk_size>>63;
    if(next_chunk_size  == 0){
      return 1;
    }
  }
  return 0;
}

/*
  Set the prev_chunk_size for chunk ch. This is an internal function
  and does not modify the prev chunk itself. Function preserves flag
  in whatever state it is in before the call
*/
void set_prev_chunk_size(struct chunk* ch, uint64_t next_chunk_size){

  assert(ch != NULL);
  
  uint64_t flag = (0x8000000000000000)&(ch->prev_chunk_size);
  
  ch->prev_chunk_size = next_chunk_size + flag;
 
  return;
}


/*
  Set the next_chunk_size for chunk ch. This is an internal function
  and does not modify the next chunk itself. Function preserves flag
  in whatever state it is in before the call
*/
void set_next_chunk_size(struct chunk* ch, uint64_t next_chunk_size){

  assert(ch != NULL);
  
  uint64_t address = (uint64_t) ch;
  address = address + ch->curr_chunk_size - 8;

  uint64_t* ptr_to_size = (uint64_t*) address;

  uint64_t flag = (0x8000000000000000)&(*ptr_to_size);

  (*ptr_to_size) = next_chunk_size + flag;
 
  return;
}

/*
  Returns the previous chunk size. Masks to exclude the available
  flag.
*/
uint64_t get_prev_chunk_size(struct chunk* ch){

  assert(ch != NULL);
  
  uint64_t prev_chunk_size = ch->prev_chunk_size;

  prev_chunk_size = prev_chunk_size & (0x7fffffffffffffff);

  return prev_chunk_size;
}

/*
  Returns the next chunk size. Masks to exclude the availabl flag.
*/
uint64_t get_next_chunk_size(struct chunk* ch){

  assert(ch != NULL);
  
  uint64_t address = (uint64_t) ch;
  address = address + ch->curr_chunk_size - 8;

  uint64_t* ptr_to_size = (uint64_t*) address;

  uint64_t next_chunk_size = (0x7fffffffffffffff)&(*ptr_to_size);

  return next_chunk_size;
}

/*
  Returns a pointer to prev chunk.
*/
struct chunk* get_prev_chunk(struct chunk* ch){

  assert(ch != NULL);
  
  uint64_t prev_address = (uint64_t)ch;

  prev_address = prev_address - get_prev_chunk_size(ch);
  
  struct chunk* prev_chunk = (struct chunk*) prev_address;

  return prev_chunk;
}

/*
  Returns a pointer to next chunk.
*/
struct chunk* get_next_chunk(struct chunk* ch){

  assert(ch != NULL);
  
  uint64_t next_address = (uint64_t)ch;

  next_address = next_address + ch->curr_chunk_size;

  struct chunk* next_chunk = (struct chunk*)next_address;

  return next_chunk;
}


/*
  Set flag bit of ch->prev_chunk_size.
  flag bit is '1' if prev_chunk is in use
  flage bit is '0' if prev_chunk is available for use
*/
void set_prev_flag(struct chunk* ch, int flag){

  assert(ch != NULL);
  
  uint64_t prev_chunk_size = (ch->prev_chunk_size);
  
  if(flag == 1){

    ch->prev_chunk_size = prev_chunk_size & 0x7fffffffffffffff;
  }
  else{
    
    ch->prev_chunk_size  = prev_chunk_size ^ 0x8000000000000000;
  }
  return;
}

/*
  Set flag bit of ch->next_chunk_size.
  flag bit is '1' if next_chunk is in use
  flage bit is '0' if next_chunk is available for use
*/
void set_next_flag(struct chunk* ch, int flag){

  assert(ch != NULL);
  
  uint64_t address = (uint64_t) ch;
  address = address + ch->curr_chunk_size - 8;

   uint64_t* ptr_to_size = (uint64_t*) address;

  if(flag == 1){

    (*ptr_to_size) = (*ptr_to_size) & 0x7fffffffffffffff;
  }
  else{
    (*ptr_to_size) = (*ptr_to_size) ^ 0x8000000000000000;
  }
  return;
}

/*
  set the previous and next chunk markers that point to 'ch' to mark
 'ch' as unavailable, if they exist.
  When unavailable flag == 1
*/
void set_unavailable(struct chunk* ch){

  assert(ch != NULL);
  
  uint64_t chunk_size = ch->curr_chunk_size;
  chunk_size = chunk_size ^ 0x8000000000000000;

  //Set prev chunk
  if(get_prev_chunk_size(ch) != 0){
    
    struct chunk* prev_chunk = get_prev_chunk(ch);
    
    set_next_flag(prev_chunk, 0);    
  }
  //Set next chunk
  if(get_next_chunk_size(ch) != 0){

    struct chunk* next_chunk = get_next_chunk(ch);
    set_prev_flag(next_chunk, 0);
  }

  return;
}

/*
  set the previous and next chunk markers that point to 'ch' to mark
 'ch' as available, if they exist.
  When available flag == 1
*/
void set_available(struct chunk* ch){

  assert(ch != NULL);
  
  uint64_t chunk_size = ch->curr_chunk_size;
  chunk_size = chunk_size & 0x7fffffffffffffff;

  //Set prev chunk
  if(get_prev_chunk_size(ch) != 0){

    struct chunk* prev_chunk = get_prev_chunk(ch);
    set_next_flag(prev_chunk, 1);
  }
  //Set next chunk
  if(get_next_chunk_size(ch) != 0){

    struct chunk* next_chunk = get_next_chunk(ch);
    set_prev_flag(next_chunk, 1);
  }

  return;
}  

/*
  If chunk 'ch' has prev and/or next neighbors set the byte in these
  neighbor chunks that point to 'ch' to ch->curr_chunk_size.
  The available flag notes if chunk 'ch' is available or not
*/
 void set_adjacent_sizes(struct chunk* ch, int available){

   assert(ch != NULL);
   
   uint64_t chunk_size;
   
   if(available == 1){
     chunk_size = ch->curr_chunk_size;
   }
   else{
     chunk_size = ch->curr_chunk_size + 0x8000000000000000;
   }

   //prev chunk
   if(ch->prev_chunk_size != 0){
     struct chunk* prev_chunk = get_prev_chunk(ch);
     set_next_chunk_size(prev_chunk, chunk_size);
   }
   //next chunk
   if(get_next_chunk_size(ch) != 0){
     struct chunk* next_chunk = get_next_chunk(ch);
     next_chunk->prev_chunk_size = chunk_size;
   }
   return;
 }
   

/*
  Find proper bin for chunk and insert into the proper place in the 
  linked list at bin.
*/
void add_chunk(struct chunk* to_add){

  assert(to_add != NULL);
  
  int i=0;
  while(to_add->curr_chunk_size > wmalloc_ptr->bin_index[i]){
    i++;
  }
 
  //push onto proper linked list
  insert_in_place(wmalloc_ptr->bin[i], to_add);
  
  return;
}

/*
  Insert in doubly linked list in proper ascending order.
*/
void insert_in_place(struct chunk* head, struct chunk* to_add){

  assert(head != NULL);
  
  struct chunk* curr = head;

  struct chunk* prev = curr;
  curr = curr->right_ptr;

  //general case
  while(curr != NULL){

    if(to_add->curr_chunk_size < curr->curr_chunk_size){

      prev->right_ptr = to_add;
      curr->left_ptr = to_add;
      to_add->right_ptr = curr;
      to_add->left_ptr = prev;
      return;
    }

    prev = curr;
    curr = curr->right_ptr;
  }

  //add as last element in list
  prev->right_ptr = to_add;
  to_add->left_ptr = prev;
  to_add->right_ptr = NULL;

  return;
}

/*
  find the bin that the requested length should be in
*/
int find_bin(uint64_t request_length){

  //find proper linked list to search
  int i=1;

  while(request_length > wmalloc_ptr->bin_index[i] && i<NUM_BINS){
    i++;
  }

  return i;
}

/*
  Search the bin for a chunk that is greater than request_length.
  If a suitable chunk is found remove and return it.
  Otherwise return NULL.
*/
struct chunk* search_bin(int i, int request_length){

  struct chunk* curr = wmalloc_ptr->bin[i];
  
  while(curr != NULL){

    if(curr->curr_chunk_size >= request_length){
      curr = remove_chunk(curr);
      break;
    }
    curr = curr->right_ptr;
  }

  return curr;
}

/*
  Search the bins of chunks starting at wmalloc_ptr->bin[i+1]
  The goal is to find the smallest chunk.
*/
struct chunk* check_bigger_bins(int i){

  struct chunk* to_remove = NULL;
  
  i++;
  while(i < NUM_BINS){
    if(wmalloc_ptr->bin[i]->right_ptr != NULL){
      to_remove = remove_chunk(wmalloc_ptr->bin[i]->right_ptr);
      break;
    }
    i++;
  }
  
  return to_remove;
}

 
/*
  The meat of the wmalloc program:
  Returns a void pointer to a chunk of at least the requested size +
  overhead.
  The wmalloc data structure is initialized if first call to wmalloc.

  Process for getting memory:
  1. Look in proper bin
  2. Look in bins of greater size
  3. Use MMAP to get more memory from OS

  Split the memory if the chunk of memory can satisfy the request and 
  has a usable amount left over as well. Reinsert the split off chunk
  and return the properly sized chunk
*/
void* wmalloc(uint64_t request_length){

  //initialize malloc struct if first time calling my_malloc
  if(wmalloc_ptr == NULL){
    if(initialize_wmalloc() == -1){
      printf("ERROR in Initializing malloc\n\n");
      return NULL;
    }
  }

  uint64_t necessary_length = request_length + CHUNK_OVERHEAD;

  //if requesting less than the minimum reset to minimum
  if(necessary_length < MINIMUM_CHUNK_SIZE){

    necessary_length = MINIMUM_CHUNK_SIZE;
  }

  int i = find_bin(necessary_length);

  struct chunk* to_remove =  search_bin(i, necessary_length);

  //check for a chunk in a bigger bin
  if(to_remove == NULL){

    to_remove = check_bigger_bins(i);
  }

  //request more memory from OS
  if(to_remove == NULL){

    to_remove =  allocate_chunk(necessary_length);

    split_chunk(to_remove, necessary_length);
  }
  else{

    split_chunk(to_remove, necessary_length);
  }

  //add 16 bytes to get to the user pointer
  uint64_t address = (uint64_t) to_remove;
  address = address+16;

  void* ret_ptr = (void*) address;

  return ret_ptr;
}


/*
  Use MMAP to get a new chunk of memory from OS
  Return a block of at least MMAP_SIZE
*/
struct chunk* allocate_chunk(uint64_t required_length){
  
  void* mmap_ptr = NULL;

  uint64_t mmap_length;
  
  if(required_length <= MMAP_SIZE){

    mmap_length = MMAP_SIZE;
  }
  else{
    //set to the smallest multiple of PAGE_SIZE that exceeds request_size
    mmap_length= (required_length/PAGE_SIZE+1)*PAGE_SIZE;
  }

  mmap_ptr = mmap(NULL, mmap_length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

  if(mmap_ptr == (void*) -1){
    printf("\n\nmmap failed\n\n");
    return NULL;
  }
 
  //cast the return of mmap to struct 'chunk'
  struct chunk* new_chunk = (struct chunk*) mmap_ptr;
    
  set_prev_chunk_size(new_chunk, 0);
  new_chunk -> curr_chunk_size = mmap_length;
  set_next_chunk_size(new_chunk, 0);
  
  return new_chunk;
}


/*
  Split the chunk if possible. 
  Return split chunk to storage bins in proper place
*/
void split_chunk(struct chunk* to_remove, uint64_t required_length){

  assert(to_remove != NULL);
  
  if(to_remove->curr_chunk_size >= required_length + MINIMUM_CHUNK_SIZE){

    uint64_t next_chunk_size = to_remove->curr_chunk_size - required_length;
    uint64_t save_chunk_size = get_next_chunk_size(to_remove);

    
    to_remove->curr_chunk_size = required_length;
    set_next_chunk_size(to_remove, next_chunk_size);

    char* char_ptr = (char*)to_remove;

    struct chunk* new_chunk = (struct chunk*)(char_ptr + to_remove->curr_chunk_size);
    new_chunk->curr_chunk_size = next_chunk_size;
    set_prev_chunk_size(new_chunk, to_remove->curr_chunk_size);
    set_next_chunk_size(new_chunk, save_chunk_size);

 
    add_chunk(new_chunk);
  }

    set_unavailable(to_remove);

  return;
}

/*
  Remove the chunk from the linked list
*/
struct chunk* remove_chunk(struct chunk* to_remove){

  struct chunk* left_neighbor = to_remove->left_ptr;
  struct chunk* right_neighbor = to_remove->right_ptr;

  if(right_neighbor == NULL){

    left_neighbor->right_ptr = NULL;
  }
  
  else{

    left_neighbor->right_ptr = right_neighbor;
    right_neighbor->left_ptr = left_neighbor;
  }

  //set 'to_remove' ptrs to NULL to avoid dangling references
  to_remove->right_ptr = NULL;
  to_remove->left_ptr = NULL;
   
  return to_remove;
}

/*
  If possible join the freed chunk with prev and next chunks.
  Then return to proper bin in the linked list 
*/  
void wfree(void* to_free){

  //subtract 16 bytes from 'to_free' and cast to struct 'chunk'
  uint64_t address = (uint64_t)(to_free);
  address = address-16;

  struct chunk* ch = (struct chunk*) address;

  //update prev and next chunks to reflect ch new status as available
  set_available(ch);

  if(is_prev_available(ch) == 1){
   
    struct chunk* prev_chunk = remove_chunk(get_prev_chunk(ch));
    ch = join_chunks(prev_chunk, ch);
    
  }
  if(is_next_available(ch) == 1){
   
    struct chunk* next_chunk = remove_chunk(get_next_chunk(ch));
    ch = join_chunks(ch, next_chunk);
  }

  add_chunk(ch);
  
  return;
}


/*
  Joins two chunks of memory that are adjacent
  'first' comes first in memory followed by 'second'
*/
 
struct chunk* join_chunks(struct chunk* first, struct chunk* second){

  uint64_t total_length = first->curr_chunk_size + second->curr_chunk_size;
  first->curr_chunk_size = total_length;

  set_adjacent_sizes(first, 1);

  return first;
}
 
#endif /*WMALLOC*/
