#include "buddy_allocator.h"
#include <stdio.h>
#include "pseudo_malloc.h"

#define BITMAP_SIZE 1 << 14
#define BUDDY_LEVELS 16
#define MEMORY_SIZE ((1024 * 1024) + 1)                 // 1MB + 1 byte per testare l'arrotondamento
#define MIN_BUCKET_SIZE (MEMORY_SIZE >> (BUDDY_LEVELS)) // 2^20 - 2^16 = 2^4 = 16

char bitmap_buffer[BITMAP_SIZE];
char memory[MEMORY_SIZE];

BuddyAllocator alloc;

int main(int argc, char **argv)
{
    // Inizializzazione del Buddy Allocator
    int num_levels = BUDDY_LEVELS;
    int memory_size = MEMORY_SIZE;
    int bitmap_buffer_size = BITMAP_SIZE;
    int min_bucket_size = MIN_BUCKET_SIZE;

    int res = BuddyAllocator_init(&alloc,
                                  num_levels,
                                  memory,
                                  memory_size,
                                  bitmap_buffer,
                                  bitmap_buffer_size,
                                  min_bucket_size);
    if (res == -1)
    {
        printf("Errore di inizializzazione del Buddy Allocator\n");
        return -1;
    }

    void *blocks[100];

    // Test casi base
    printf("\n----- TEST CASI BASE -----\n");
    pseudo_malloc(&alloc, 0);
    pseudo_malloc(&alloc, -1);

    // Test allocazione blocchi di dimensione variabile
    printf("\n----- TEST ALLOCAZIONE BLOCCHI DI DIMENSIONE VARIABILE -----\n");
    for (int i = 0; i < 20; i++)
    {
        blocks[i] = pseudo_malloc(&alloc, (1 << i) % 5000);
    }

    // Test deallocazione completa
    printf("\n----- TEST DEALLOCAZIONE COMPLETA -----\n");
    for (int i = 0; i < 20; i++)
    {
        pseudo_free(&alloc, &blocks[i]);
    }

    // Test double free
    printf("\n----- TEST DOUBLE FREE -----\n");
    for (int i = 0; i < 20; i++)
    {
        pseudo_free(&alloc, &blocks[i]); // Deve generare errori
    }

    // Test riallocaizone dopo deallocazione completa
    printf("\n----- TEST RIALLOCAZIONE DOPO DEALLOCAZIONE COMPLETA -----\n");
    void *test_block2 = pseudo_malloc(&alloc, 120000);
    if (test_block2)
    {
        printf("[SUCCESSO] Riallocazione riuscita dopo la deallocazione completa.\n");
        pseudo_free(&alloc, &test_block2);
    }
    else
    {
        printf("[ERRORE] Riallocazione fallita: la memoria non è stata completamente liberata!\n");
    }

    // Test allocazione di elevata dimensione
    printf("\n----- TEST ALLOCAZIONE DI ELEVATA DIMENSIONE -----\n");
    void *big_block = pseudo_malloc(&alloc, MEMORY_SIZE * 2); //  metà della memoria totale
    if (big_block)
    {
        printf("[SUCCESSO] Allocazione di %d bytes riuscita\n", MEMORY_SIZE * 2);
        pseudo_free(&alloc, &big_block);
    }
    else
    {
        printf("[ERRORE] Allocazione di %d bytes fallita!\n", MEMORY_SIZE * 2);
    }

    void *big_block2 = pseudo_malloc(&alloc, MEMORY_SIZE * 20);
    if (big_block2)
    {
        printf("[SUCCESSO] Allocazione di %d bytes riuscita\n", MEMORY_SIZE * 20);
        pseudo_free(&alloc, &big_block2);
    }
    else
    {
        printf("[ERRORE] Allocazione di %d bytes fallita!\n", MEMORY_SIZE * 20);
    }

    void *big_block3 = pseudo_malloc(&alloc, MEMORY_SIZE * 2000);
    if (big_block3)
    {
        printf("[SUCCESSO] Allocazione di %d bytes riuscita\n", MEMORY_SIZE * 2000);
        pseudo_free(&alloc, &big_block3);
    }
    else
    {
        printf("[ERRORE] Allocazione di %d bytes fallita!\n", MEMORY_SIZE * 2000);
    }

    // Test allocaizone e deallocazione alternata
    printf("\n----- TEST ALLOCAZIONE E DEALLOCAZIONE ALTERNATA -----\n");
    void *alt_blocks[10];
    for (int i = 0; i < 10; i++)
    {
        alt_blocks[i] = pseudo_malloc(&alloc, 500 + (i * 100)); // Allocazioni di dimensione crescente
        if (alt_blocks[i])
        {
            printf("[SUCCESSO] Allocazione %d: %p\n", i, alt_blocks[i]);
        }
        else
        {
            printf("[ERRORE] Allocazione %d fallita!\n", i);
        }

        if (i % 2 == 1)
        { // Ogni secondo blocco viene deallocato subito
            pseudo_free(&alloc, &alt_blocks[i]);
            alt_blocks[i] = NULL;
        }
    }

    // Dealloca il resto
    for (int i = 0; i < 10; i++)
    {
        if (alt_blocks[i])
        {
            pseudo_free(&alloc, &alt_blocks[i]);
        }
    }

    return 0;
}
