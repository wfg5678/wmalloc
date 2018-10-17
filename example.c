#include <stdio.h>
#include <stdlib.h>
#include "wmalloc.h"
#include <time.h>


int main(){

  srand(time(NULL));

  int* array = wmalloc(sizeof(int)*100000);
  
  printf("Here is the address of the array: %p\n", array);

  for(int i=0; i<100000; i++){
    array[i] = rand();
  }

  //print first few elements
  for(int i=0; i<10; i++){
    printf("%d \n", array[i]);
  }

  wfree(array);
  
  return 0;
}
