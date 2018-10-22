/*

  Some tests to determine the speed and correctness of wmalloc.
  Tests are conducted with wmalloc and the stdlib malloc for compar-
  son.

  wmalloc is slow compared with stdlib malloc when allocating and freeing
  many small chunks of memory. My guess is that wmalloc aggressively 
  attempts to consolidate freed chunks.

  wmalloc and stdlib malloc are very similar when allocating and freeing 
  chunks of around one page size. wmalloc seems to outperform stdlib malloc
  slightly in this test.

  The two memory allocators perform similarly on large allocations. This makes
  sense because a large allocation is simply a call to mmap with some 
  decoration on top.

  The differences in performance show the trade offs inherent in designing an
  allocator. The different patterns of allocation make it difficult to make
  a perfect allocator for all situations.

*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "wmalloc.h"


/*
  Debugging function:
  Calculates the total memory obtained from OS and not yet doled out
*/
uint64_t calc_mem_available(){

    uint64_t total=0;
  struct chunk* curr;
  for(int i=0; i<NUM_BINS; i++){
    
    curr = wmalloc_ptr->bin[i]->right_ptr;
    while(curr != NULL){
      total = total + curr->curr_chunk_size;
      curr = curr->right_ptr;
    }
  }
  return total;
}

/*
  Debugging function:
  Shows available chunks of memory by bucket
*/
void print_available(){

  struct chunk* curr;
  for(int i=0; i<NUM_BINS; i++){
    printf("less than %lu - ", wmalloc_ptr->bin_index[i]);
    curr = wmalloc_ptr->bin[i]->right_ptr;
    while(curr != NULL){
      printf(" %lu", curr->curr_chunk_size);
      curr = curr->right_ptr;
    }
    printf("\n");
  }
  return;
}

/*
  Allocate memory for 5000 of 10000 pointer array.
  Then "flip a coin" to determine whether to add another chunk or
  free a chunk. Do this a 10000 times. Then free the memory in the
  array
*/
void wmalloc_test1(){

  int r;
  int index = 0;

  void* array[10000];

  for(int i=0; i<5000; i++){

    r = rand()%0x1000;
    array[index] = wmalloc(r);
    index++;
  }

  //flip a coin to determine whether to malloc or free
  int coinflip;

  for(int i=0; i<10000; i++){

    coinflip = rand()%2;
    if(coinflip == 0){
      index--;
      wfree(array[index]);
    }
    else{
      r = rand()%0x1000;
      array[index] = wmalloc(r);
      index++;
    }
  }
  for(int i=index-1; i>=0; i--){
    wfree(array[i]);
  }
  return;
}

/*
  The same as wmalloc_test1 but with stdlib malloc instead of wmalloc
*/
void std_test1(){

  int r;
  int index = 0;

  void* array[10000];

  for(int i=0; i<5000; i++){

    r = rand()%0x1000;
    array[index] = malloc(r);
    index++;
  }
  
  int coinflip;

  for(int i=0; i<10000; i++){

    coinflip = rand()%2;
    if(coinflip == 0){
      index--;
      free(array[index]);
    }
    else{
      r = rand()%(0x1000);
      array[index] = malloc(r);
      index++;
    }
  }

  for(int i=index-1; i>=0; i--){
    free(array[i]);

  }
  return;
}

/*
  allocate 1 million size(int) chunks
*/
void wmalloc_test2(){

  int** array = wmalloc(sizeof(int*)*1000000);
  for(int i=0; i<1000000; i++){

    array[i] = wmalloc(sizeof(int));
  }

  for(int i=0; i<1000000; i++){
    wfree(array[i]);
  }

  wfree(array);
}

//uses malloc instead of wmalloc for performance comparison
void std_test2(){

  int** array = malloc(sizeof(int*)*1000000);
  for(int i=0; i<1000000; i++){

    array[i] = malloc(sizeof(int));
  }

  for(int i=0; i<1000000; i++){
    free(array[i]);
  }

  free(array);
}

int main(){

  srand(time(NULL));
 
  clock_t t;
  double time_taken;
   
  t = clock(); 
  wmalloc_test1(); 
  t = clock() - t; 
  time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds 
  
  printf("wmalloc_test1() took %f seconds to execute \n", time_taken);

  t = clock(); 
  std_test1(); 
  t = clock() - t; 
  time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds 
  
  printf("std_test1() took %f seconds to execute \n", time_taken);

  t = clock(); 
  wmalloc_test2(); 
  t = clock() - t; 
  time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds 
  
  printf("wmalloc_test2() took %f seconds to execute \n", time_taken);

  t = clock(); 
  std_test2(); 
  t = clock() - t; 
  time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds 
  
  printf("std_test2() took %f seconds to execute \n", time_taken);
  
  return 0;
}
