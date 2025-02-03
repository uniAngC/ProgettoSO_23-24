#include <stdio.h>
#include <assert.h>
#include "pseudo_malloc.h"
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#define MAP_ANONYMOUS 0x20 // Definizione manuale, altrimenti errore

// Funzione per l'allocazione della memoria
void *pseudo_malloc(BuddyAllocator *alloc, int size)
{
    if (size < 0)
    {
        printf("[ERRORE] pseudo_malloc: dimensione non valida (<0)\n");
        return NULL;
    }
    if (size == 0)
    {
        printf("[ERRORE] pseudo_malloc: impossibile allocare 0 byte\n");
        return NULL;
    }

    // Se la dimensione supera una soglia, si usa mmap per allocare la memoria
    if (size >= THRESHOLD)
    {
        printf("[INFO] mmap: richiesta Dimensione: %d bytes (+%lu bytes overhead)\n", size, sizeof(int));
        int memory_size = size + sizeof(int); // per salvare la dimensione, si aggiunge 4 bytes di overhead
        void *p = mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (p == MAP_FAILED)
        {
            printf("[ERRORE] mmap: %s\n", strerror(errno));
            return NULL;
        }
        ((int *)p)[0] = memory_size; // Salvataggio della dimensione originale
        printf("[SUCCESSO] mmap: allocazione Indirizzo: %p, Dimensione: %d bytes\n", p + sizeof(int), memory_size);
        return (void *)(p + sizeof(int));
    }
    else
    {
        void *p = BuddyAllocator_malloc(alloc, size);
        if (!p)
        {
            return NULL;
        }
        return p;
    }
}

// Funzione per la deallocazione della memoria (non viene contato l'overhead)
void pseudo_free(BuddyAllocator *alloc, void **mem)
{
    if (!(*mem))
    {
        printf("[ERRORE] Deallocazione fallita: puntatore NULL fornito\n");
        return;
    }

    // Tramite la variabile che tiene salvata la dimensione del blocco (overhead escluso)
    // controlla e dealloca con munmap o BuddyAllocator_free
    int *p = (int *)(*mem);
    int size = *(--p);
    if (size >= THRESHOLD)
    {
        printf("[INFO] Deallocazione richiesta con munmap\n");
        int ret = munmap((void *)p, size);
        if (ret != 0)
        {
            printf("[ERRORE] Deallocazione fallita: %s\n", strerror(errno));
            return;
        }
        printf("[SUCCESSO] Deallocazione con munmap. Blocco liberato: %p\n", *mem);
        *mem = NULL; // per evitare segmantation fault nei test double free
    }
    else
    {
        printf("[INFO] Deallocazione richiesta con Buddy Allocator\n");
        BuddyAllocator_free(alloc, *mem);
    }
}
