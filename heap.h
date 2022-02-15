#ifndef INC_1_7_PROJEKT_HEAP_H
#define INC_1_7_PROJEKT_HEAP_H


struct memory_manager_t
{
    void *memory_start;
    size_t memory_size;
    int init_flag;
    struct memory_chunk_t *first_memory_chunk;
};

struct memory_chunk_t
{
    struct memory_chunk_t* prev;
    struct memory_chunk_t* next;
    size_t size;
    int free;
    int control_size_flag;
};


enum pointer_type_t
{
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};


int heap_setup(void);
void heap_clean(void);
int heap_validate(void);

void* heap_malloc(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t count);
void  heap_free(void* memblock);

size_t   heap_get_largest_used_block_size(void);

enum pointer_type_t get_pointer_type(const void* const pointer);

void* heap_malloc_aligned(size_t count);
void* heap_calloc_aligned(size_t number, size_t size);
void* heap_realloc_aligned(void* memblock, size_t size);
//pomocnicze

void defragmentation();
void delete_free(struct memory_chunk_t* temp);
void control();
int set_flag(struct memory_chunk_t*curr);

#endif
