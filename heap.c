#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "custom_unistd.h"
#include "heap.h"
#include <stdlib.h>
#include <time.h>
#include "tested_declarations.h"
#include "rdebug.h"

#define PAGE_SIZE 4096
#define FENCE_SIZE 4
#define PTR_TO_BLOCK_START(ptr) ((char*)(ptr)+sizeof(struct memory_chunk_t)+FENCE_SIZE)
#define ROUND(ptr,count_of_pages) ((((size_t)(ptr) + (4096*(count_of_pages)))/4096)*4096)

struct memory_manager_t memoryManager;

int heap_setup(void)
{
    void * first_time_request = custom_sbrk(0);
    //sbrk nie zwraca NULLa tylko -1
    if(first_time_request == (void*)-1)return -1;
    //inicjowanie sterty
    memoryManager.memory_start = first_time_request;
    memoryManager.memory_size=0;
    memoryManager.first_memory_chunk=NULL;
    memoryManager.init_flag=69;

    return 0;
}
void heap_clean(void)
{
    //oddawanie całej pamieci do systemu
    custom_sbrk(-1*(long)memoryManager.memory_size);
    // wyzerowanie bloków kontorolnych
    memoryManager.first_memory_chunk=NULL;
    memoryManager.init_flag=0;
    memoryManager.memory_start=NULL;
}


void* heap_malloc(size_t size)
{
    if(memoryManager.memory_start==NULL || size<=0)return NULL;
    //tworzenie pierwszego bloku pamieci
    if(memoryManager.first_memory_chunk == NULL)
    {
        //pobieranie pamieci od systemu i sprawdzanie czy pamiec zostala przydzielona
        struct memory_chunk_t* first_chunk = (struct memory_chunk_t*)custom_sbrk((long)(sizeof(struct memory_chunk_t)+size+2*FENCE_SIZE));
        if(first_chunk == (void*)-1)return NULL;

        //zapisywanie obszaru sterty
        memoryManager.memory_size+=(sizeof(struct memory_chunk_t)+size+2*FENCE_SIZE);
        //inicjalizacja bloku
        first_chunk->size=size;
        first_chunk->prev=NULL;
        first_chunk->next=NULL;
        first_chunk->free=0;

        //flaga do sprawdzania czy struktura nie jest zamazana


        //plotki ploteczki
        memset((char*)first_chunk+ sizeof(struct memory_chunk_t),0,FENCE_SIZE);
        memset((char*)first_chunk+ sizeof(struct memory_chunk_t)+FENCE_SIZE+size,0,FENCE_SIZE);
        //ustawianie first_chunka
        memoryManager.first_memory_chunk = first_chunk;

        //zwracanie adresu poczatku bloku
        first_chunk->control_size_flag = set_flag(memoryManager.first_memory_chunk);
        return PTR_TO_BLOCK_START(memoryManager.first_memory_chunk);
    }
    //sprawdzamy poprawnosc sterty
    if(heap_validate() != 0)return NULL;

    //inicjalizaacja pointerów
    struct memory_chunk_t* prev = memoryManager.first_memory_chunk->prev;
    struct memory_chunk_t* curr = memoryManager.first_memory_chunk;
    struct memory_chunk_t * next = memoryManager.first_memory_chunk->next;

    while(curr!=NULL)
    {
        //znajdowanie wolnego bloku ktory pomiesci blok alokowany w danym momencie
        if(curr->free == 1 && curr->size>= size+2*FENCE_SIZE)
        {
            curr->size = size;
            curr->prev = prev;
            curr->next = next;
            curr->free = 0;

            memset((char*)curr+ sizeof(struct memory_chunk_t),0,FENCE_SIZE);
            memset((char*)curr +sizeof(struct memory_chunk_t)+FENCE_SIZE+size,0,FENCE_SIZE);

            curr->control_size_flag = set_flag(curr);
            return PTR_TO_BLOCK_START(curr);
        }

        if(next == NULL)break;
        prev = curr;
        curr=next;
        next=next->next;
    }
    //pobieranie pamieci dla nowego bloku
    struct memory_chunk_t* new_chunk = (struct memory_chunk_t*)custom_sbrk((long)(sizeof(struct memory_chunk_t)+size+2*FENCE_SIZE));

    if(new_chunk== (void*)-1)return NULL;

    new_chunk->next=NULL;
    new_chunk->prev=curr;
    new_chunk->free=0;
    new_chunk->size=size;
    //zwiekszenie pamięci
    memoryManager.memory_size+=(sizeof(struct memory_chunk_t)+size+2*FENCE_SIZE);

    memset((char*)new_chunk+ sizeof(struct memory_chunk_t),0,FENCE_SIZE);
    memset((char*)new_chunk+sizeof(struct memory_chunk_t)+FENCE_SIZE+size,0,FENCE_SIZE);

    curr->next= new_chunk;

    curr->next->control_size_flag = set_flag(curr->next);
    curr->control_size_flag = set_flag(curr);

    return PTR_TO_BLOCK_START(curr->next);
}
void* heap_calloc(size_t number, size_t size)
{
    if(number<=0 || size<=0)return NULL;
    void * to_calloc = heap_malloc(number*size);
    if(to_calloc == NULL)return NULL;

    memset((char*)to_calloc,0,number*size);

    return to_calloc;
}
void* heap_realloc(void* memblock, size_t count)
{
    if(memblock == NULL || memoryManager.first_memory_chunk==NULL)return heap_malloc(count);
    if(heap_validate()!=0)return NULL;


    struct memory_chunk_t* curr = memoryManager.first_memory_chunk;
    struct memory_chunk_t *next = memoryManager.first_memory_chunk->next;
    int search_flag=0;

    while(curr!=NULL)
    {
        //szukamy odpowiedniej pozycji w pamieci
        if((char*)memblock - (char*)curr == sizeof(struct memory_chunk_t)+FENCE_SIZE)
        {
            search_flag=1;
            break;
        }
        curr = next;
        if(curr == NULL)break;
        next=next->next;
    }
    //brak znalezionego bloku
    if(search_flag == 0)
    {
        control();
        return NULL;
    }
    if(count == 0)
    {
        heap_free(memblock);
        return NULL;
    }
    if(curr->size > count)
    {
        curr->size = count;
        memset((char*)curr+ sizeof(struct memory_chunk_t)+FENCE_SIZE+curr->size,0,FENCE_SIZE);
        control();
        return PTR_TO_BLOCK_START(curr);
    }
    else if(curr->size == count)return memblock;
    else
    {
        if(next!=NULL)
        {
            if(((char*)next+ sizeof(struct memory_chunk_t)+next->size+2*FENCE_SIZE - (char*)curr) - sizeof(struct memory_chunk_t)- 2*FENCE_SIZE >= count)
            {

                curr->size = count;
                curr->next=next->next;
                next->prev = curr;
                memset((char*)curr+ sizeof(struct memory_chunk_t)+FENCE_SIZE+curr->size,0,FENCE_SIZE);
                control();
                return PTR_TO_BLOCK_START(curr);
            }
            else
            {

                void * alloc_new = heap_malloc(count);
                if(alloc_new == NULL)return NULL;
                memcpy((char*)alloc_new,(char*)curr+ sizeof(struct memory_chunk_t)+FENCE_SIZE,curr->size);

                heap_free((char*)curr+ sizeof(struct memory_chunk_t)+FENCE_SIZE);

                return alloc_new;
            }
        }

        void*temp = custom_sbrk((long)(count - curr->size));
        if(temp==(void*)-1)return NULL;

        memoryManager.memory_size+= (count-curr->size);
        curr->size = count;
        memset((char*)curr+ sizeof(struct memory_chunk_t)+FENCE_SIZE+curr->size,0,FENCE_SIZE);
        control();
        return PTR_TO_BLOCK_START(curr);

    }
}
void heap_free(void* memblock)
{
    if(memblock == NULL || heap_validate()!=0)return;
    if(memoryManager.memory_start==NULL || memoryManager.first_memory_chunk==NULL)return;

    struct memory_chunk_t* curr = memoryManager.first_memory_chunk;

    int search_flag=0;

    while(curr!=NULL)
    {
        //szukamy odpowiedniej pozycji w pamieci
        if((char*)memblock - (char*)curr == sizeof(struct memory_chunk_t)+FENCE_SIZE)
        {
            search_flag=1;
            break;
        }

        curr=curr->next;
    }
    //brak znalezionego bloku
    if(search_flag == 0)
    {
        control();
        return;
    }

    if(curr == memoryManager.first_memory_chunk && curr->next == NULL)
    {
        memoryManager.first_memory_chunk =NULL;
        return;
    }

    curr->free = 1;
    if (curr->next != NULL)
        curr->size = (char *) curr->next - ((char *) curr + sizeof(struct memory_chunk_t));
    else
        curr->size = memoryManager.memory_size - ((unsigned long)((char *) curr - (char *) memoryManager.memory_start) +sizeof(struct memory_chunk_t));

    defragmentation();
    control();
}

void defragmentation()
{
    struct  memory_chunk_t * temp  = memoryManager.first_memory_chunk;
    if(temp==NULL)return;
    while(temp->next!=NULL)
    {
        if(temp->free==1 && temp->next->free==1)
        {
            temp->size += temp->next->size + sizeof(struct memory_chunk_t)+2*FENCE_SIZE;
            temp->next = temp->next->next;
        }
        else
            temp=temp->next;
    }
}
void control()
{
    struct memory_chunk_t* curr = memoryManager.first_memory_chunk;

    while(curr!=NULL)
    {
        curr->control_size_flag = set_flag(curr);
        curr=curr->next;
    }
}
size_t  heap_get_largest_used_block_size(void)
{
    if(heap_validate()!=0)return 0;
    if(memoryManager.first_memory_chunk==NULL)return 0;
    struct memory_chunk_t* curr = memoryManager.first_memory_chunk;
    size_t max = 0;
    while(curr!=NULL)
    {
        if(curr->size > max && curr->free ==0)max = curr->size;
        curr=curr->next;
    }
    return max ;
}

enum pointer_type_t get_pointer_type(const void* const pointer)
{
    if(pointer == NULL)return pointer_null;
    if(memoryManager.first_memory_chunk == NULL)return pointer_unallocated;

    if(heap_validate() !=0)return pointer_heap_corrupted;

    struct memory_chunk_t* curr = memoryManager.first_memory_chunk;
    while(curr!=NULL)
    {
        if(curr->free == 0) {
            if ((char *) curr + sizeof(struct memory_chunk_t) > (char *) pointer)return pointer_control_block;
            else if (((char *) curr + sizeof(struct memory_chunk_t) + FENCE_SIZE > (char *) pointer))
                return pointer_inside_fences;
            else if ((char *) curr + sizeof(struct memory_chunk_t) + FENCE_SIZE == (char *) pointer)
                return pointer_valid;
            else if ((char *) curr + sizeof(struct memory_chunk_t) + FENCE_SIZE + curr->size > (char *) pointer)
                return pointer_inside_data_block;
            else if (((char *) curr + sizeof(struct memory_chunk_t) + FENCE_SIZE + curr->size + FENCE_SIZE >
                      (char *) pointer))
                return pointer_inside_fences;
            else if(curr->next !=NULL && (char*)curr->next > (char*)pointer && ((char *) curr + sizeof(struct memory_chunk_t) + FENCE_SIZE + curr->size + FENCE_SIZE <=
                                                                                (char *) pointer))return pointer_unallocated;
        }
        else if(((char *) curr + sizeof(struct memory_chunk_t) + curr->size>
                 (char *) pointer))return pointer_unallocated;
        curr=curr->next;
    }
    return pointer_unallocated;
}


int heap_validate(void)
{
    if(memoryManager.init_flag != 69)return 2;
    struct memory_chunk_t* curr = memoryManager.first_memory_chunk;
    while (curr!=NULL)
    {
        if(curr->control_size_flag != set_flag(curr))return 3;
        if(curr->free == 0)
        {
            for (int i = 0; i < FENCE_SIZE; i++) {
                if (*((char *) curr + sizeof(struct memory_chunk_t) + i) != 0)
                    return 1;
                if (*((char *) curr + sizeof(struct memory_chunk_t) + FENCE_SIZE + curr->size + i) != 0)
                    return 1;
            }
        }
        curr=curr->next;
    }
    return 0;
}
int set_flag(struct memory_chunk_t*curr)
{
    int set_flag=0;
    for(int i=0;i<7;i++)
    {
        set_flag+= *((int*)((char*)(curr)+ i * sizeof(int)));
    }
    return set_flag;
}
void* heap_malloc_aligned(size_t count)
{
    if(memoryManager.memory_start==NULL || count<=0)return NULL;
    //tworzenie pierwszego bloku pamieci
    if(memoryManager.first_memory_chunk == NULL)
    {
        //pobieranie pamieci od systemu i sprawdzanie czy pamiec zostala przydzielona
        void *temp= custom_sbrk((long) (PAGE_SIZE + count + FENCE_SIZE));
        if(temp == (void*)-1)return NULL;
        temp = (void*)((char*) ROUND(temp,1)- sizeof(struct memory_chunk_t)-FENCE_SIZE);
        struct memory_chunk_t*first_chunk = temp;
        //zapisywanie obszaru sterty
        memoryManager.memory_size+=(PAGE_SIZE+count+FENCE_SIZE);
        //inicjalizacja bloku
        first_chunk->size=count;
        first_chunk->prev=NULL;
        first_chunk->next=NULL;
        first_chunk->free=0;

        //flaga do sprawdzania czy struktura nie jest zamazana


        //plotki ploteczki
        memset((char*)first_chunk+ sizeof(struct memory_chunk_t),0,FENCE_SIZE);
        memset((char*)first_chunk+ sizeof(struct memory_chunk_t)+FENCE_SIZE+count,0,FENCE_SIZE);
        //ustawianie first_chunka
        memoryManager.first_memory_chunk = first_chunk;

        //zwracanie adresu poczatku bloku
        first_chunk->control_size_flag = set_flag(memoryManager.first_memory_chunk);
        return PTR_TO_BLOCK_START(memoryManager.first_memory_chunk);
    }
    //sprawdzamy poprawnosc sterty
    if(heap_validate() != 0)return NULL;

    //inicjalizaacja pointerów
    struct memory_chunk_t* prev = memoryManager.first_memory_chunk->prev;
    struct memory_chunk_t* curr = memoryManager.first_memory_chunk;
    struct memory_chunk_t * next = memoryManager.first_memory_chunk->next;

    while(curr!=NULL)
    {
        //znajdowanie wolnego bloku ktory pomiesci blok alokowany w danym momencie
        if(curr->free == 1 && curr->size>= count+2*FENCE_SIZE && ((intptr_t) PTR_TO_BLOCK_START(curr) & (intptr_t)(PAGE_SIZE - 1)) == 0)
        {
            curr->size = count;
            curr->prev = prev;
            curr->next = next;
            curr->free = 0;
            //custom_sbrk(-1*(long)(curr->size-count-2*FENCE_SIZE));
            //memoryManager.memory_size -= curr->size-count-2*FENCE_SIZE;
            memset((char*)curr+ sizeof(struct memory_chunk_t),0,FENCE_SIZE);
            memset((char*)curr +sizeof(struct memory_chunk_t)+FENCE_SIZE+count,0,FENCE_SIZE);

            curr->control_size_flag = set_flag(curr);
            return PTR_TO_BLOCK_START(curr);
        }

        if(next == NULL)break;
        prev = curr;
        curr=next;
        next=next->next;
    }
    //pobieranie pamieci dla nowego bloku
    void*temp=NULL;
    size_t check = (size_t)((intptr_t)ROUND(custom_sbrk(0),1)-(intptr_t)custom_sbrk(0));
    if(check>=sizeof(struct memory_chunk_t)+FENCE_SIZE)
    {
        temp = custom_sbrk((long)(check+count+FENCE_SIZE));
        if(temp== (void*)-1)return NULL;

        memoryManager.memory_size+=(check+count+FENCE_SIZE);
        temp = (void*)((char*) ROUND(temp,1) - sizeof(struct memory_chunk_t)-FENCE_SIZE);
    }

    else
    {
        temp = custom_sbrk((long)(check+PAGE_SIZE + count + FENCE_SIZE));
        if(temp== (void*)-1)return NULL;

        memoryManager.memory_size+=(check+PAGE_SIZE + count + FENCE_SIZE);
        temp = (void*)((char*) ROUND(temp,2) - sizeof(struct memory_chunk_t)-FENCE_SIZE);
    }
    struct memory_chunk_t*new_chunk = (struct memory_chunk_t*)temp;
    new_chunk->next=NULL;
    new_chunk->prev=curr;
    new_chunk->free=0;
    new_chunk->size=count;

    memset((char*)new_chunk+ sizeof(struct memory_chunk_t),0,FENCE_SIZE);
    memset((char*)new_chunk+sizeof(struct memory_chunk_t)+FENCE_SIZE+count,0,FENCE_SIZE);

    curr->next= new_chunk;

    curr->next->control_size_flag = set_flag(curr->next);
    curr->control_size_flag = set_flag(curr);

    return PTR_TO_BLOCK_START(curr->next);

}
void* heap_calloc_aligned(size_t number, size_t size)
{
    if(number<=0 || size<=0)return NULL;
    void * to_calloc = heap_malloc_aligned(number*size);
    if(to_calloc == NULL)return NULL;

    memset((char*)to_calloc,0,number*size);

    return to_calloc;
}
void* heap_realloc_aligned(void* memblock, size_t count)
{

    if(memblock == NULL || memoryManager.first_memory_chunk==NULL)return heap_malloc_aligned(count);
    if(heap_validate()!=0)return NULL;

    if(count == 0)
    {
        heap_free(memblock);
        return NULL;
    }

    struct memory_chunk_t* curr = memoryManager.first_memory_chunk;
    struct memory_chunk_t *next = memoryManager.first_memory_chunk->next;
    int search_flag=0;

    while(curr!=NULL)
    {
        //szukamy odpowiedniej pozycji w pamieci
        if((char*)memblock - (char*)curr == sizeof(struct memory_chunk_t)+FENCE_SIZE)
        {
            search_flag=1;
            break;
        }
        curr = next;
        if(curr == NULL)break;
        next=next->next;
    }
    //brak znalezionego bloku
    if(search_flag == 0)
    {
        control();
        return NULL;
    }
    if((intptr_t)memblock & (intptr_t)(PAGE_SIZE-1))
    {
        void * allig = heap_malloc_aligned(count);
        if(!allig)return NULL;

        memcpy(allig,memblock,curr->size);
        heap_free(memblock);
        return allig;
    }
    if(curr->size > count)
    {
        curr->size = count;
        memset((char*)curr+ sizeof(struct memory_chunk_t)+FENCE_SIZE+curr->size,0,FENCE_SIZE);
        control();
        return PTR_TO_BLOCK_START(curr);
    }
    else if(curr->size == count)return memblock;
    else
    {
        if(next!=NULL)
        {
            if(((char*)next+ sizeof(struct memory_chunk_t)+next->size+2*FENCE_SIZE - (char*)curr) - sizeof(struct memory_chunk_t)- 2*FENCE_SIZE >= count)
            {

                curr->size = count;
                curr->next=next->next;
                next->prev = curr;
                memset((char*)curr+ sizeof(struct memory_chunk_t)+FENCE_SIZE+curr->size,0,FENCE_SIZE);
                control();
                return PTR_TO_BLOCK_START(curr);
            }
            else
            {

                void * alloc_new = heap_malloc_aligned(count);
                if(alloc_new == NULL)return NULL;
                memcpy((char*)alloc_new,(char*)curr+ sizeof(struct memory_chunk_t)+FENCE_SIZE,curr->size);

                heap_free((char*)curr+ sizeof(struct memory_chunk_t)+FENCE_SIZE);

                return alloc_new;
            }
        }

        void*temp = custom_sbrk((long)(count - curr->size));
        if(temp==(void*)-1)return NULL;

        memoryManager.memory_size+= (count-curr->size);
        curr->size = count;
        memset((char*)curr+ sizeof(struct memory_chunk_t)+FENCE_SIZE+curr->size,0,FENCE_SIZE);
        control();
        return PTR_TO_BLOCK_START(curr);

    }
}




