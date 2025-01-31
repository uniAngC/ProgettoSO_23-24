#include "buddy_allocator.h"
#include <stdio.h>

#define BITMAP_SIZE 1 << 14 // 16KB
#define BUDDY_LEVELS 16
#define MEMORY_SIZE ((1024 * 1024) + 1)                 // 1MB + 1 byte per testare l'arrotondamento
#define MIN_BUCKET_SIZE (MEMORY_SIZE >> (BUDDY_LEVELS)) // 2^20 - 2^16 = 2^4 = 16

char bitmap_buffer[BITMAP_SIZE];
char memory[MEMORY_SIZE];

BuddyAllocator alloc;

int main(int argc, char **argv)
{
    // Inizializzazione
    int num_levels = BUDDY_LEVELS;
    int memory_size = MEMORY_SIZE;
    int bitmap_buffer_size = BITMAP_SIZE;
    int min_bucket_size = MIN_BUCKET_SIZE;
    int res = BuddyAllocator_init(&alloc, num_levels, memory, memory_size, bitmap_buffer, bitmap_buffer_size, min_bucket_size);

    if (res == -1)
    {
        printf("Errore di inizializzazione del Buddy Allocator\n");
        return -1;
    }

    void *blocks[100];

    // Test caasi base
    printf("\n----- TEST CASI BASE -----\n");
    BuddyAllocator_malloc(&alloc, 0);
    BuddyAllocator_malloc(&alloc, -1);
    BuddyAllocator_malloc(&alloc, MEMORY_SIZE + 1);

    // Test riempimento completo della memoria
    printf("\n----- TEST RIEMPIMENTO COMPLETO MEMORIA -----\n");
    for (int i = 0; i < 10; i++)
    {
        blocks[i] = BuddyAllocator_malloc(&alloc, 120000);
    }

    // Test allocazione con MEMORIA PIENA (deve fallire)
    printf("\n----- TEST ALLOCAZIONE QUANDO MEMORIA PIENA -----\n");
    void *test_block = BuddyAllocator_malloc(&alloc, 10);
    if (test_block == NULL)
    {
        printf("[SUCCESSO] Nessun blocco disponibile, test superato\n");
    }
    else
    {
        printf("[ERRORE] L'allocazione è riuscita quando non doveva! Indirizzo: %p\n", test_block);
    }

    // Deallocazione completa
    printf("\n----- TEST DEALLOCAZIONE COMPLETA -----\n");
    for (int i = 0; i < 10; i++)
    {
        BuddyAllocator_free(&alloc, blocks[i]);
    }

    // Controllo: Se tutto è stato deallocato, dovremmo riuscire a riallocare lo stesso spazio
    printf("\n----- TEST RIALLOCAZIONE DOPO DEALLOCAZIONE COMPLETA -----\n");
    void *test2_block = BuddyAllocator_malloc(&alloc, 120000);
    if (test2_block)
    {
        printf("[SUCCESSO] Riallocazione riuscita dopo la deallocazione completa.\n");
        BuddyAllocator_free(&alloc, test2_block);
    }
    else
    {
        printf("[ERRORE] Riallocazione fallita: la memoria non è stata completamente liberata!\n");
    }

    // Test DOUBLE FREE (deve fallire)
    printf("\n----- TEST DOUBLE FREE -----\n");
    for (int i = 0; i < 10; i++)
    {
        BuddyAllocator_free(&alloc, blocks[i]);
    }

    // Test allocazione random dopo il reset della memoria
    printf("\n----- TEST ALLOCAZIONE BLOCCHI RANDOM -----\n");
    for (int i = 0; i < 20; i++)
    {
        blocks[i] = BuddyAllocator_malloc(&alloc, 1 << i);
    }

    // Deallocazione blocchi random in ordine inverso
    printf("\n----- TEST DEALLOCAZIONE BLOCCHI RANDOM IN ORDINE INVERSO -----\n");
    for (int i = 19; i >= 0; i--)
    {
        BuddyAllocator_free(&alloc, blocks[i]);
    }

    return 0;
}
