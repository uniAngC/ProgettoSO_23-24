#include <stdio.h>
#include <assert.h>
#include "pseudo_malloc.h"
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#define MAP_ANONYMOUS 0x20 // altrimenti errore. Provato a risolvere, nulla, dovuto definire manualmente

// Funzione per l'allocazione della memoria
void *pseudo_malloc(BuddyAllocator *alloc, int size)
{
    if (size < 0)
    {
        printf("[ERRORE2] Allocazione fallita: dimensione non valida (<0)\n");
        return NULL;
    }
    if (size == 0)
    {
        printf("[ERRORE2] Allocazione fallita: impossibile allocare 0 byte\n");
        return NULL;
    }

    // Se la dimensione supera una soglia, si usa mmap per allocare la memoria
    if (size >= THRESHOLD)
    {
        printf("[INFO2] Allocazione richiesta con mmap. Dimensione: %d bytes\n", size);
        int memory_size = size + sizeof(int); // per salvare la dimensione
        void *p = mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (p == MAP_FAILED)
        {
            printf("[ERRORE2] mmap fallita: %s\n", strerror(errno));
            return NULL;
        }
        ((int *)p)[0] = memory_size; // Salvataggio della dimensione originale
        printf("[SUCCESSO2] Allocazione con mmap. Indirizzo: %p, Dimensione: %d bytes\n", p + sizeof(int), memory_size);
        return (void *)(p + sizeof(int));
    }
    else
    {
        printf("[INFO2] Allocazione richiesta con Buddy Allocator. Dimensione: %d bytes\n", size);
        void *p = BuddyAllocator_malloc(alloc, size);
        if (!p)
        {
            printf("[ERRORE2] Allocazione fallita: Buddy Allocator esaurito\n");
            return NULL;
        }
        printf("[SUCCESSO2] Allocazione con Buddy Allocator. Indirizzo: %p\n", p);
        return p;
    }
}

// Funzione per la deallocazione della memoria
void pseudo_free(BuddyAllocator *alloc, void **mem)
{
    if (!(*mem))
    {
        printf("[ERRORE2] Deallocazione fallita: puntatore NULL fornito\n");
        return;
    }

    // Tramite la variabile che tiene salvata la dimensione del blocco (overhead escluso)
    // controlla e dealloca con munmap o BuddyAllocator_free
    int *p = (int *)(*mem);
    int size = *(--p);
    if (size >= THRESHOLD)
    {
        printf("[INFO2] Deallocazione richiesta con munmap\n");
        int ret = munmap((void *)p, size);
        if (ret != 0)
        {
            printf("[ERRORE2] Deallocazione fallita: munmap failed\n");
            return;
        }
        printf("[SUCCESSO2] Deallocazione con munmap. Blocco liberato: %p\n", *mem);
        *mem = NULL;
    }
    else
    {
        printf("[INFO2] Deallocazione richiesta con Buddy Allocator\n");
        BuddyAllocator_free(alloc, *mem);
    }
}
