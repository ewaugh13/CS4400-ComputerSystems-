/*
 * mm-naive.c - The least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by allocating a
 * new page as needed.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused.
 *
 * The heap check and free check always succeeds, because the
 * allocator doesn't depend on any of the old data.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* always use 16-byte alignment */
#define ALIGNMENT 16

#define PAGESIZE (mem_pagesize()) //<1

typedef struct {
  size_t size;
  char   allocated;
} block_header;

typedef struct {
  size_t size;
  char filler;
} block_footer;

typedef struct {
  size_t size;
  size_t filler;
  void *prev;
  void *next;
} chunk_header;

#define GET_ALLOC(p) ((block_header *)(p))->allocated

/* determines the size of the allocated payload */
#define GET_SIZE(p) ((block_header *)(p))->size

/* the size of the header and footer */
#define OVERHEAD (sizeof(block_header)+sizeof(block_footer))

/* getting the header from a payload ptr */
#define HDRP(bp) ((char *)(bp) - sizeof(block_header))

/* getting the next blocks payload */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))

/* jumping back to the previous block ptr */
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-OVERHEAD))

/* the ptr to the footer of the current block adding the header but removing OVERHEAD */
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-OVERHEAD)

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

/* rounds up to the nearest multiple of mem_pagesize() */
#define PAGE_ALIGN(size) (((size) + (mem_pagesize()-1)) & ~(mem_pagesize()-1))

/* rounds down to the nearest multiple of mem_pagesize() */
#define ADDRESS_PAGE_START(p) ((void *)(((size_t)p) & ~(mem_pagesize()-1)))

unsigned long MAXSIZE = 1UL << 32;
static chunk_header *start_chunk = NULL;
static chunk_header *most_recent_chunk = NULL;

void print_memory_atbp(void *bp)
{
  void *current_bp = bp;
  while(GET_SIZE(HDRP(current_bp)) != 0)
  {
    printf("%p current bp", current_bp);
    size_t size_words = GET_SIZE(HDRP(current_bp))/sizeof(block_header);
    printf("[%zu,%d]==[%zu]--> ", size_words, GET_ALLOC(HDRP(current_bp)), size_words);
    current_bp = NEXT_BLKP(current_bp);
  }
  printf("[0,1]\n");
}

void print_memory()
{
  if(start_chunk != NULL)
  {
    chunk_header *current_chunk = start_chunk;
    void *current_bp = ((void*)current_chunk) + sizeof(block_header) + sizeof(chunk_header);
    print_memory_atbp(current_bp);
    while(current_chunk->next != NULL)
    {
      current_chunk = current_chunk->next;
      current_bp  = ((void*)current_chunk) + sizeof(block_header) + sizeof(chunk_header);
      print_memory_atbp(current_bp);
    }
  }
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  void* current_ptr = mem_map(PAGESIZE);
  start_chunk = current_ptr;
  start_chunk->size = PAGESIZE; start_chunk->next = NULL; start_chunk->prev = NULL;
  most_recent_chunk = start_chunk;

  // setting up the prolouge
  current_ptr += sizeof(block_header) + sizeof(chunk_header);
  GET_SIZE(HDRP(current_ptr)) = OVERHEAD;
  GET_ALLOC(HDRP(current_ptr)) = 1;
  GET_SIZE(FTRP(current_ptr)) = OVERHEAD;

  // setting up beginning of the payload to have a header and footer
  size_t payload_size = PAGESIZE - OVERHEAD - sizeof(block_header) - sizeof(chunk_header);
  GET_SIZE(HDRP(NEXT_BLKP(current_ptr))) = payload_size; //everything besides null term and prolouge and chunk header
  GET_ALLOC(HDRP(NEXT_BLKP(current_ptr))) = 0;
  GET_SIZE(FTRP(NEXT_BLKP(current_ptr))) = payload_size;

  //setting up null term
  void* end_chunk;
  end_chunk = current_ptr + PAGESIZE - sizeof(block_header) - sizeof(chunk_header); //need to get to end of page but remove the block_header so HDRP can get the header [][][][]<-
  GET_SIZE(HDRP(end_chunk)) = 0;
  GET_ALLOC(HDRP(end_chunk)) = 1;

  current_ptr += OVERHEAD;
    
  return 0;
}

void *extend(size_t new_size) 
{
  size_t chunk_size = PAGE_ALIGN(80 * new_size + sizeof(chunk_header) + sizeof(block_header) + sizeof(block_footer) + sizeof(block_header)); //adding the prolouge and the null terminator size
  void *current_ptr = mem_map(chunk_size);
  if(start_chunk == NULL)
  {
    start_chunk = current_ptr;
    start_chunk->prev = NULL;
  }
  chunk_header *current_chunk = start_chunk;
  chunk_header *next_chunk = current_ptr;
  while(current_chunk->next != NULL)
  {
    current_chunk = current_chunk->next;
  }
  next_chunk->size = chunk_size;
  current_chunk->next = next_chunk;
  next_chunk->prev = current_chunk; next_chunk->next = NULL;
  most_recent_chunk = next_chunk;

  // setting up the prolouge
  current_ptr += sizeof(block_header) + sizeof(chunk_header);
  GET_SIZE(HDRP(current_ptr)) = OVERHEAD;
  GET_ALLOC(HDRP(current_ptr)) = 1;
  GET_SIZE(FTRP(current_ptr)) = OVERHEAD;

  // setting up beginning of the payload to have a header and footer
  size_t payload_size = chunk_size - OVERHEAD - sizeof(block_header) - sizeof(chunk_header);
  GET_SIZE(HDRP(NEXT_BLKP(current_ptr))) = payload_size; //everything besides null term and prolouge
  GET_ALLOC(HDRP(NEXT_BLKP(current_ptr))) = 0;
  GET_SIZE(FTRP(NEXT_BLKP(current_ptr))) = payload_size;

  //setting up null term
  void* end_chunk;
  end_chunk = current_ptr + chunk_size - sizeof(block_header) - sizeof(chunk_header); //need to get to end of page but remove the block_header so HDRP can get the header [][][][]<-

  GET_SIZE(HDRP(end_chunk)) = 0;
  GET_ALLOC(HDRP(end_chunk)) = 1;

  return NEXT_BLKP(current_ptr);
}

/* 
 * set_allocated - sets the current payload pointer to the size we are asking for and if there is extra set up new header and footer
 */
void set_allocated(void *bp, size_t size) 
{
  size_t extra_size = GET_SIZE(HDRP(bp)) - size; //size remaining if what we allocate something smaller
  if (extra_size > ALIGN(1 + OVERHEAD)) //can it fit the overhead and some memory itself
  {
    GET_SIZE(HDRP(bp)) = size;
    GET_SIZE(FTRP(bp)) = size;
    GET_SIZE(HDRP(NEXT_BLKP(bp))) = extra_size;
    GET_ALLOC(HDRP(NEXT_BLKP(bp))) = 0;
    GET_SIZE(FTRP(NEXT_BLKP(bp))) = extra_size;
  }
  GET_ALLOC(HDRP(bp)) = 1;
}

/* 
 * mm_malloc - Allocate a block by using bytes from current_ptr,
 *     grabbing a new page if necessary.
 */
void *mm_malloc(size_t size)
{
  int new_size = ALIGN(size + OVERHEAD); //makes it aligned on 16
  if(most_recent_chunk != NULL && most_recent_chunk != start_chunk)
  {
    chunk_header *current_chunk = most_recent_chunk;
    void* current_bp = ((void*)current_chunk) + sizeof(block_header) + sizeof(chunk_header);
    while (GET_SIZE(HDRP(current_bp)) != 0) 
    {
      if (!GET_ALLOC(HDRP(current_bp)) && (GET_SIZE(HDRP(current_bp)) >= new_size)) 
      {
        set_allocated(current_bp, new_size);
        return current_bp;
      }
      current_bp = NEXT_BLKP(current_bp);
    }
  }
  void *current_bp = NULL;
  if(start_chunk != NULL)
  {
    chunk_header *current_chunk = start_chunk;
    current_bp = ((void*)current_chunk) + sizeof(block_header) + sizeof(chunk_header);
    while (GET_SIZE(HDRP(current_bp)) != 0) 
    {
      if (!GET_ALLOC(HDRP(current_bp)) && (GET_SIZE(HDRP(current_bp)) >= new_size)) 
      {
        set_allocated(current_bp, new_size);
        most_recent_chunk = current_chunk; 
        return current_bp;
      }
      current_bp = NEXT_BLKP(current_bp);
    }
    while(current_chunk->next != NULL)
    {
      current_chunk = current_chunk->next;
      current_bp  = ((void*)current_chunk) + sizeof(block_header) + sizeof(chunk_header);
      while (GET_SIZE(HDRP(current_bp)) != 0) 
      {
        if (!GET_ALLOC(HDRP(current_bp)) && (GET_SIZE(HDRP(current_bp)) >= new_size)) 
        {
          set_allocated(current_bp, new_size);
          most_recent_chunk = current_chunk; 
          return current_bp;
        }
        current_bp = NEXT_BLKP(current_bp);
      }
    }
  }

  current_bp = extend(new_size);
  set_allocated(current_bp, new_size);

  return current_bp;
}

void *coalesce(void *bp) 
{
  size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));   
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) /* Case 1 */
  {
    /* nothing to do */
  }
  else if (prev_alloc && !next_alloc) /* Case 2 coalese next block*/
  {
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    GET_SIZE(HDRP(bp)) = size;
    GET_SIZE(FTRP(bp)) = size;
  }

  else if (!prev_alloc && next_alloc) /* Case 3 coalese previous block */
  {
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    GET_SIZE(FTRP(bp)) = size;
    GET_SIZE(HDRP(PREV_BLKP(bp))) = size;
    bp = PREV_BLKP(bp);
  }

  else /* Case 4 coalese both blocks*/
  {
    size += (GET_SIZE(HDRP(PREV_BLKP(bp)))
             + GET_SIZE(HDRP(NEXT_BLKP(bp))));
    GET_SIZE(HDRP(PREV_BLKP(bp))) = size;
    GET_SIZE(FTRP(NEXT_BLKP(bp))) = size;
    bp = PREV_BLKP(bp);
  }
  //check if we can do a mem unmap
  size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
  size_t prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
  
  if(next_size == 0 && prev_size == 32)
  {
    chunk_header *current_chunk = (chunk_header*)(PREV_BLKP(bp) - sizeof(chunk_header) - sizeof(block_header));
    chunk_header *next_chunk = (chunk_header*)(current_chunk->next);
    chunk_header *prev_chunk = (chunk_header*)(current_chunk->prev);
    if(next_chunk == NULL && current_chunk == start_chunk)
    {
      start_chunk = NULL;
    }
    else if(next_chunk != NULL && prev_chunk != NULL) //sets next and prev to point to each other
    {
      prev_chunk->next = (void*)(next_chunk);
      next_chunk->prev = (void*)(prev_chunk);
    }
    else if(prev_chunk != NULL) //when prev_chunk is start_chunk and current_chunk is second one
    {
      prev_chunk->next = NULL;
    }
    else if(next_chunk != NULL && current_chunk == start_chunk) //removeing current chunk == start_chunk next becomes start_chunk
    {
      start_chunk = next_chunk;
      start_chunk->prev = NULL;
    }
    mem_unmap((void*)current_chunk, current_chunk->size);
    if(current_chunk == most_recent_chunk)
    {
      most_recent_chunk = NULL;
    }
  }
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{ 
  GET_ALLOC(HDRP(bp)) = 0;
  coalesce(bp);
}

int ptr_is_mapped(void *p, size_t len) 
{
  void *s = ADDRESS_PAGE_START(p);
  return mem_is_mapped(s, PAGE_ALIGN((p + len) - s));
}

/*
 * mm_check - Check whether the heap is ok, so that mm_malloc()
 *            and proper mm_free() calls won't crash.
 */
int mm_check()
{
  if(start_chunk != NULL)
  {
    chunk_header *current_chunk = start_chunk;
    void *current_bp = ((void*)current_chunk) + sizeof(block_header) + sizeof(chunk_header);
    while(current_chunk->next != NULL)
    {
      if(!ptr_is_mapped((void *)current_chunk, current_chunk->size))
      {
        return 0;
      }
      current_bp  = ((void*)current_chunk) + sizeof(block_header) + sizeof(chunk_header);
      if(!ptr_is_mapped(current_bp, GET_SIZE(HDRP(current_bp))))
      {
	return 0;
      }
      //check for prolouge
      if(GET_SIZE(HDRP(current_bp)) != OVERHEAD || GET_ALLOC(HDRP(current_bp)) != 1)
      {
        return 0;
      }
      if(GET_SIZE(FTRP(current_bp)) != OVERHEAD)
      {
        return 0;
      }
      current_bp = NEXT_BLKP(current_bp);
      if(!ptr_is_mapped(current_bp, GET_SIZE(HDRP(current_bp))))
      {
	return 0;
      }
      while (GET_SIZE(HDRP(current_bp)) != 0) 
      {
	if(!ptr_is_mapped(HDRP(current_bp), GET_SIZE(HDRP(current_bp))))
        {
	  return 0;
	}
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(current_bp)));
        if(next_size > MAXSIZE)
        {
          return 0;
        }
	if(GET_SIZE(HDRP(current_bp)) != GET_SIZE(FTRP(current_bp)))
        {
          return 0;
        }
        current_bp = NEXT_BLKP(current_bp);
      }

      if(!ptr_is_mapped(HDRP(current_bp), GET_SIZE(HDRP(current_bp))))
      {
	return 0;
      }
    
      if(GET_SIZE(HDRP(current_bp)) != 0 || GET_ALLOC(HDRP(current_bp)) != 1) //checks for null terminator
      {
        return 0;
      }
       
      current_chunk = current_chunk->next;
      if(!ptr_is_mapped((void*)current_chunk, mem_pagesize()))
      {
	  return 0;
      }
      if(!ptr_is_mapped((void*)current_chunk, current_chunk->size))
      {
	return 0;
      }
      if(current_chunk->next != NULL)
      {
	if(!ptr_is_mapped(current_chunk->next, mem_pagesize()))
	{
	  return 0;
	}
	if(!ptr_is_mapped(current_chunk->next, ((chunk_header*)current_chunk->next)->size))
	{
	  return 0;
	}
      }
    }

    if(!ptr_is_mapped((void *)current_chunk, current_chunk->size))
    {
      return 0;
    }
    
    current_bp  = ((void*)current_chunk) + sizeof(block_header) + sizeof(chunk_header);
    
    //check for prolouge
    if(GET_SIZE(HDRP(current_bp)) != OVERHEAD || GET_ALLOC(HDRP(current_bp)) != 1)
    {
      return 0;
    }
    if(GET_SIZE(FTRP(current_bp)) != OVERHEAD)
    {
      return 0;
    }
    current_bp = NEXT_BLKP(current_bp);

    while (GET_SIZE(HDRP(current_bp)) != 0) 
    {
      if(!ptr_is_mapped(HDRP(current_bp), GET_SIZE(HDRP(current_bp))))
      {
	return 0;
      }
      size_t current_size = GET_SIZE(HDRP(current_bp));
      if(current_size > MAXSIZE)
      {
	return 0;
      }
      
      size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(current_bp)));
      if(next_size > MAXSIZE)
      {
        return 0;
      }
      if(GET_SIZE(HDRP(current_bp)) != GET_SIZE(FTRP(current_bp)))
      {
        return 0;
      }
      current_bp = NEXT_BLKP(current_bp);
    }
    if(!ptr_is_mapped(HDRP(current_bp), GET_SIZE(HDRP(current_bp))))
    {
      return 0;
    }
    if(GET_SIZE(HDRP(current_bp)) != 0 || GET_ALLOC(HDRP(current_bp)) != 1) //checks for null terminator
    {
      return 0;
    }
  }
  
  return 1;
}

/*
 * mm_check - Check whether freeing the given `p`, which means that
 *            calling mm_free(p) leaves the heap in an ok state.
 */
int mm_can_free(void *p)
{
  if(start_chunk != NULL)
  {
    chunk_header *current_chunk = start_chunk;
    void *current_bp = ((void*)current_chunk) + sizeof(block_header) + sizeof(chunk_header);
    while(current_chunk->next != NULL)
    {
      if(!ptr_is_mapped((void *)current_chunk, current_chunk->size))
      {
        return 0;
      }
      current_bp  = ((void*)current_chunk) + sizeof(block_header) + sizeof(chunk_header);
      if(current_bp == p)
      {
    	if(!ptr_is_mapped(p, GET_SIZE(HDRP(p))))
  	{
	  return 0;
  	}
       	if(GET_ALLOC(HDRP(p)) == 0)
	{
	  return 0;
      	}
	if(GET_SIZE(HDRP(p)) != GET_SIZE(FTRP(p)))
       	{
	  return 0;
       	}
      }

      current_bp = NEXT_BLKP(current_bp);
    
      while (GET_SIZE(HDRP(current_bp)) != 0) 
      {
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(current_bp)));
        if(next_size > MAXSIZE)
        {
          return 0;
        }
        if(current_bp == p)
	{
	if(!ptr_is_mapped(p, GET_SIZE(HDRP(p))))
	{
	  return 0;
	}
	if(GET_ALLOC(HDRP(p)) == 0)
	{
	  return 0;
	}
	if(GET_SIZE(HDRP(p)) != GET_SIZE(FTRP(p)))
	{
	  return 0;
	}
	return 1;
      }
        if(GET_SIZE(HDRP(current_bp)) != GET_SIZE(FTRP(current_bp)))
        {
          return 0;
        }
        current_bp = NEXT_BLKP(current_bp);
      }
    
      current_chunk = current_chunk->next;
    }

    if(!ptr_is_mapped((void *)current_chunk, current_chunk->size))
    {
      return 0;
    }
    
    current_bp  = ((void*)current_chunk) + sizeof(block_header) + sizeof(chunk_header);

    current_bp = NEXT_BLKP(current_bp);

    while (GET_SIZE(HDRP(current_bp)) != 0) 
    {
      size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(current_bp)));
      if(next_size > MAXSIZE)
      {
        return 0;
      }
      if(current_bp == p)
      {
	if(!ptr_is_mapped(p, GET_SIZE(HDRP(p))))
      	{
	  return 0;
      	}
	if(GET_ALLOC(HDRP(p)) == 0)
       	{
	  return 0;
       	}
	if(GET_SIZE(HDRP(p)) != GET_SIZE(FTRP(p)))
       	{
	  return 0;
       	}
	return 1;
      }
      if(GET_SIZE(HDRP(current_bp)) != GET_SIZE(FTRP(current_bp)))
      {
        return 0;
      }
      current_bp = NEXT_BLKP(current_bp);
      if(!ptr_is_mapped(current_bp, GET_SIZE(HDRP(current_bp))))
      {
      	return 0;
      }
    }
  }
  return 0;
}
