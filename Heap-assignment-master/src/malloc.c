#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN4(s)         (((((s) - 1) >> 2) << 2) + 4)
#define BLOCK_DATA(b)     ((b) + 1)
#define BLOCK_HEADER(ptr) ((struct _block *)(ptr) - 1)

static int atexit_registered = 0;
static int num_mallocs       = 0;
static int num_frees         = 0;
static int num_reuses        = 0;
static int num_grows         = 0;
static int num_splits        = 0;
static int num_coalesces     = 0;
static int num_blocks        = 0;
static int num_requested     = 0;
static int max_heap          = 0;

void printStatistics( void )
{
  printf("\nheap management statistics\n");
  printf("mallocs:\t\t%d\n", num_mallocs );
  printf("frees:\t\t%d\n", num_frees );
  printf("reuses:\t\t%d\n", num_reuses );
  printf("grows:\t\t%d\n", num_grows );
  printf("splits:\t\t%d\n", num_splits );
  printf("coalesces:\t%d\n", num_coalesces );
  printf("blocks:\t\t%d\n", num_blocks );
  printf("requested:\t%d\n", num_requested );
  printf("max heap:\t%d\n", max_heap );
}

struct _block 
{
   size_t  size;
   struct _block *next;
   bool   free;
   char   padding[3];
};

struct _block *heapList = NULL;
static struct _block *last_used = NULL;

struct _block *findFreeBlock(struct _block **last, size_t size) 
{
   struct _block *curr = heapList;

#if defined FIT && FIT == 0
   while (curr && !(curr->free && curr->size >= size)) 
   {
      *last = curr;
      curr  = curr->next;
   }
#endif

#if defined BEST && BEST == 0
   struct _block *best = NULL;
   while (curr) {
      if (curr->free && curr->size >= size) {
         if (!best || curr->size < best->size) {
            best = curr;
         }
      }
      *last = curr;
      curr = curr->next;
   }
   curr = best;
#endif

#if defined WORST && WORST == 0
   struct _block *worst = NULL;
   while (curr) {
      if (curr->free && curr->size >= size) {
         if (!worst || curr->size > worst->size) {
            worst = curr;
         }
      }
      *last = curr;
      curr = curr->next;
   }
   curr = worst;
#endif

#if defined NEXT && NEXT == 0
   if (!last_used) last_used = heapList;
   struct _block *start = last_used;
   do {
      if (last_used->free && last_used->size >= size) {
         curr = last_used;
         break;
      }
      *last = last_used;
      last_used = last_used->next ? last_used->next : heapList;
   } while (last_used != start);
#endif

   return curr;
}

struct _block *growHeap(struct _block *last, size_t size) 
{
   struct _block *curr = (struct _block *)sbrk(0);
   struct _block *prev = (struct _block *)sbrk(sizeof(struct _block) + size);

   assert(curr == prev);

   if (curr == (struct _block *)-1) 
   {
      return NULL;
   }

   if (heapList == NULL) 
   {
      heapList = curr;
   }

   if (last) 
   {
      last->next = curr;
   }

   curr->size = size;
   curr->next = NULL;
   curr->free = false;
   num_grows++;
   num_blocks++;
   max_heap += size;
   return curr;
}

void *malloc(size_t size) 
{
   printf("✅ malloc called! size = %zu\n", size);

   if( atexit_registered == 0 )
   {
      atexit_registered = 1;
      atexit( printStatistics );
   }

   size = ALIGN4(size);

   if (size == 0) 
   {
      return NULL;
   }

   num_requested += size;
   num_mallocs++;

   struct _block *last = heapList;
   struct _block *next = findFreeBlock(&last, size);

   if (next) {
      num_reuses++;
      if (next->size >= size + sizeof(struct _block) + 4) {
         struct _block *split = (struct _block *)((char *)BLOCK_DATA(next) + size);
         split->size = next->size - size - sizeof(struct _block);
         split->free = true;
         split->next = next->next;
         next->size = size;
         next->next = split;
         num_splits++;
         num_blocks++;
      }
   } else {
      next = growHeap(last, size);
      if (!next) return NULL;
   }

   next->free = false;
   last_used = next;
   return BLOCK_DATA(next);
}

void free(void *ptr) 
{
   if (ptr == NULL) 
   {
      return;
   }

   struct _block *curr = BLOCK_HEADER(ptr);
   assert(curr->free == 0);
   curr->free = true;
   num_frees++;

   struct _block *tmp = heapList;
   while (tmp && tmp->next) {
      if (tmp->free && tmp->next->free) {
         tmp->size += sizeof(struct _block) + tmp->next->size;
         tmp->next = tmp->next->next;
         num_coalesces++;
         num_blocks--;
      } else {
         tmp = tmp->next;
      }
   }
}

void *calloc( size_t nmemb, size_t size )
{
   size_t total = nmemb * size;
   void *ptr = malloc(total);
   if (ptr) memset(ptr, 0, total);
   return ptr;
}

void *realloc( void *ptr, size_t size )
{
   if (!ptr) return malloc(size);
   if (size == 0) {
      free(ptr);
      return NULL;
   }
   struct _block *curr = BLOCK_HEADER(ptr);
   if (curr->size >= size) return ptr;

   void *new_ptr = malloc(size);
   if (!new_ptr) return NULL;

   memcpy(new_ptr, ptr, curr->size);
   free(ptr);
   return new_ptr;
}
// Add this at the end of your program
extern void printStatistics(void);
