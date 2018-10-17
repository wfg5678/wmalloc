#include <stdio.h>
#include <stdlib.h>
#include "wmalloc.h"
#include <time.h>


//test 1
void test1(){

  int r;
  int index;

  void* array[1000];

  for(int i=0; i<500; i++){

    r = rand()%0x10000;
    array[i] = wmalloc(r);
    printf("adding %d to pos %d\n", r, i);
  }
  index = 500;

  int coinflip;

  for(int i=0; i<1000; i++){

    coinflip = rand()%2;
    if(coinflip == 0){
      index--;
      wfree(array[index]);
      printf("freeing from pos %d\n", index); 
    }
    else{
      r = rand()%0x10000;
      array[index] = wmalloc(r);
      printf("adding %d to pos %d\n", r, index);
      index++;
    }
  }

  for(int i=index-1; i>=0; i--){
    printf("freeing pos %d\n", i);
    wfree(array[i]);
    
  }
  return;
}

//test 2
void test2(){

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
void standard2(){

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
 
  test2();
  
  return 0;
}
