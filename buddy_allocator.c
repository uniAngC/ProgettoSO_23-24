#include <stdio.h>
#include <assert.h>
#include <math.h> // for floor and log2
#include "buddy_allocator.h"

///////////////////////////////////////////////////////////
// these are trivial helpers to support you in case you want
// to do a bitmap implementation

// level of node i
int levelIdx(size_t idx)
{
    return (int)floor(log2(idx + 1));
};
// index of the buddy of node i
int buddyIdx(int idx)
{
    if (idx == 0) // radice
        return 0;
    if (idx % 2) // se pari
        return idx - 1;
    return idx + 1; // altrimenti se dispari
}
// parent of the node idx
int parentIdx(int idx)
{
    return (int)(idx - 1) / 2;
}
// idx of 1st node of a level i
int firstIdx(int level)
{
    return (1 << level) - 1;
}
// offset of node idx in his level
int startIdx(int idx)
{
    return (idx - (firstIdx(levelIdx(idx))));
}
///////////////////////////////////////////////////////////

int BuddyAllocator_init(BuddyAllocator *alloc,
                        int num_levels,
                        char *memory,
                        int memory_size,
                        char *bitmap_buffer,
                        int bitmap_buffer_size,
                        int min_bucket_size)
{
    if (num_levels >= MAX_LEVELS)
    {
        printf("[ERRORE] Creazione Buddy Allocator: Numero di livelli superiore al massimo consentito (%d)\n", MAX_LEVELS);
        return -1;
    }

    if (!memory)
    {
        printf("[ERRORE] Creazione Buddy Allocator: Puntatore alla memoria fornita è NULL\n");
        return -1;
    }

    if (memory_size <= 0)
    {
        printf("[ERRORE] Creazione Buddy Allocator: La dimensione della memoria deve essere maggiore di 0\n");
        return -1;
    }

    if (!bitmap_buffer)
    {
        printf("[ERRORE] Creazione Buddy Allocator: Puntatore al buffer per la bitmap è NULL\n");
        return -1;
    }

    if (bitmap_buffer_size <= 0)
    {
        printf("[ERRORE] Creazione Buddy Allocator: La dimensione del buffer per la bitmap deve essere maggiore di 0\n");
        return -1;
    }

    // Controllo sulla consistenza di min_bucket_size
    if (min_bucket_size != memory_size >> num_levels)
    {
        printf("[ERRORE] Creazione Buddy Allocator: min_bucket_size non è valido rispetto alla dimensione della memoria e al numero di livelli\n");
        return -1;
    }

    // Arrotondamento della memoria alla potenza di 2 più vicina
    if (log2(memory_size) != floor(log2(memory_size)))
    {
        memory_size = min_bucket_size << num_levels;
        printf("[INFO] Memoria arrotondata a potenza di 2: %d bytes\n", memory_size);
    }

    alloc->num_levels = num_levels;
    alloc->memory = memory;
    alloc->memory_size = memory_size;
    alloc->min_bucket_size = min_bucket_size;

    // Calcolo e inizializzazione della bitmap
    int num_bits = (1 << (num_levels + 1)) - 1;
    if (bitmap_buffer_size < BitMap_getBytes(num_bits))
    {
        printf("[ERRORE] Creazione Buddy Allocator: Memoria fornita insufficiente per contenere la bitmap (richiesti: %d bytes)\n", BitMap_getBytes(num_bits));
        return -1;
    }

    BitMap_init(&(alloc->bitmap), num_bits, bitmap_buffer);
    printf("\n[INFO] Buddy Allocator Inizializzato con successo:\n");
    printf("- Livelli: %d\n", num_levels);
    printf("- Dimensione della Memoria: %d bytes\n", memory_size);
    printf("- Numero di bit nella Bitmap: %d\n", num_bits);
    printf("- Dimensione della Bitmap: %d bytes\n", BitMap_getBytes(num_bits));
    printf("- Dimensione minima dei blocchi: %d bytes\n", min_bucket_size);
    return 0;
}

// Cerca un buddy libero da restituire alla malloc
void *BuddyAllocator_getBuddy(BuddyAllocator *alloc, int level, int size)
{
    int bitmap_idx = -1;
    if (level == 0)
    {
        int bit = BitMap_bit(&alloc->bitmap, firstIdx(level));
        if (bit == 0)
            bitmap_idx = 0;
    }
    else
    {
        int i = firstIdx(level);
        while (i < firstIdx(level + 1))
        {
            int bit = BitMap_bit(&alloc->bitmap, i);
            if (bit == 0)
            {
                bitmap_idx = i;
                break;
            }
            i++;
        }
    }
    if (bitmap_idx == -1)
    {
        return NULL;
    }
    else
    {
        update_child(&alloc->bitmap, bitmap_idx, 1);
        update_parent(&alloc->bitmap, bitmap_idx, 1);

        int block_size = alloc->min_bucket_size << (alloc->num_levels - level);

        char *ret = alloc->memory + ((startIdx(bitmap_idx) + 1) * block_size);

        ((int *)ret)[0] = bitmap_idx;
        ((int *)ret)[1] = size;
        return (void *)(ret + 2 * sizeof(int));
    }
}

void *BuddyAllocator_malloc(BuddyAllocator *alloc, int size)
{
    if (size < 0)
    {
        printf("[ERRORE] malloc: Dimensione non valida\n");
        return NULL;
    }
    if (size == 0)
    {
        printf("[ERRORE] malloc: Impossibile allocare 0 byte\n");
        return NULL;
    }

    int org_size = size;
    size += 2 * sizeof(int);

    if (size > alloc->memory_size)
    {
        printf("[ERRORE] malloc: Memoria richiesta (%d bytes) supera la memoria disponibile (%d bytes)\n", size, alloc->memory_size);
        return NULL;
    }

    int mem_size = (1 << alloc->num_levels) * alloc->min_bucket_size;
    int level = floor(log2(mem_size / size));
    if (level > alloc->num_levels)
    {
        level = alloc->num_levels;
    }

    printf("[INFO] Allocazione richiesta: %d bytes (+%lu bytes overhead), livello: %d\n",
           org_size, 2 * sizeof(int), level);

    void *indirizzo = BuddyAllocator_getBuddy(alloc, level, org_size);
    if (indirizzo == NULL)
    {
        printf("[ERRORE] malloc: Nessun blocco di memoria libero disponibile\n");
    }
    else
    {
        printf("[SUCCESSO] malloc: Allocazione completata: indirizzo %p, dimensione %d bytes\n", indirizzo, alloc->min_bucket_size << (alloc->num_levels - level));
        return indirizzo;
    }
    return NULL;
}

void BuddyAllocator_free(BuddyAllocator *alloc, void *mem)
{
    if (!mem)
    {
        printf("[ERRORE] Puntatore NULL fornito a free\n");
        return;
    }

    // ricerca del bit del buddy, precendentemente salvato in memoria
    int *p = (int *)mem;
    p--;              // salto della size precedente salvata nel blocco
    int idx = *(--p); // indice della bitmap del blocco
    BuddyAllocator_releaseBuddy(alloc, idx, mem);
}

void BuddyAllocator_releaseBuddy(BuddyAllocator *alloc, int bit, void *mem)
{
    if (BitMap_bit(&alloc->bitmap, bit) == 0)
    {
        printf("[ERRORE] Double free detected per il blocco: %p\n", mem);
        return;
    }
    update_child(&alloc->bitmap, bit, 0);
    merge(&alloc->bitmap, bit);
    printf("[SUCCESSO] Blocco deallocato con successo: %p\n", mem);
}

void merge(BitMap *bitmap, int bit)
{
    if (bit == 0)
        return; // radice

    int value = BitMap_bit(bitmap, bit);
    if (value == 1)
        printf("[ERRORE] bitmap: merge su bit 1\n");
    return;

    int buddy = buddyIdx(bit);
    value = BitMap_bit(bitmap, buddy);
    // controllo se il buddy è libero o no
    if (value == 1)
    {
        return; // non è libero, non si fa nulla
    }
    else
    { // imposta il bit del padre a 0 facendo il merge dei figli ricorsivamente
        int parent = parentIdx(bit);
        BitMap_setBit(bitmap, parent, 0);
        merge(bitmap, parent); // ricorsione in risalita
    }
}

// imposta il bit stesso e il bit del padre nella bitmap al valore passato come argomento 1 oppure 0
void update_parent(BitMap *bitmap, int bit, int value)
{
    BitMap_setBit(bitmap, bit, value);
    if (bit > 0)
    {
        update_parent(bitmap, parentIdx(bit), value); // ricorsione in salita
    }
}

void update_child(BitMap *bitmap, int bit, int value)
{
    if (bit < bitmap->num_bits)
    {
        BitMap_setBit(bitmap, bit, value);
        // ricorsione sui figli in discesa
        update_child(bitmap, bit * 2 + 1, value); // sinistro
        update_child(bitmap, bit * 2 + 2, value); // destro
    }
}