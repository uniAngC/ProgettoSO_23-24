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
    if (idx == 0)
        return 0;
    if (idx & 0x1)
        return idx - 1;
    return idx + 1;
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
    // check on num_levels
    assert("Numero di Livelli Superiore al Massimo (20 -> 1MB)" && num_levels < MAX_LEVELS);

    // check on min_bucket_size
    assert("Dimensione del min_bucket size troppo piccola (<8)" && min_bucket_size >= 8);

    // se la memoria non è una potenza di 2, verrà utilizzata la porzione di memoria
    // più grande possibile (disponibile) che è potenza di 2
    if (log2(memory_size) != floor(log2(memory_size)))
    {
        memory_size = min_bucket_size << num_levels;
        printf("[INFO] Memoria utilizzata: %d bytes (arrotondata a potenza di 2)\n", memory_size);
    }

    alloc->num_levels = num_levels;
    alloc->memory = memory;
    alloc->memory_size = memory_size;
    alloc->min_bucket_size = min_bucket_size;

    // check e inizializzazione bitmap
    int num_bits = (1 << (num_levels + 1)) - 1;
    assert("Memoria insufficiente per contenere la bitmap" && bitmap_buffer_size >= BitMap_getBytes(num_bits));

    BitMap_init(&(alloc->bitmap), num_bits, bitmap_buffer);
    printf("\n[Buddy Allocator Inizializzato]\n");
    printf("- Livelli: %d\n", num_levels);
    printf("- Dimensione della Memoria: %d bytes\n", memory_size);
    printf("- Numero di bit nella Bitmap: %d\n", num_bits);
    printf("- Dimensione della Bitmap: %d bytes\n", BitMap_getBytes(num_bits));
    printf("- Dimensione minima dei blocchi: %d bytes\n\n", min_bucket_size);
    return 1;
}

void *BuddyAllocator_getBuddy(BuddyAllocator *alloc, int level, int size)
{
    int idx = -1;
    if (level == 0)
    {
        int bit = BitMap_bit(&alloc->bitmap, firstIdx(level));
        if (bit == 0)
            idx = 0;
    }
    else
    {
        int i = firstIdx(level);
        while (i < firstIdx(level + 1))
        {
            int bit = BitMap_bit(&alloc->bitmap, i);
            if (bit == 0)
            {
                idx = i;
                break;
            }
            i++;
        }
    }
    if (idx == -1)
    {
        return NULL;
    }
    else
    {
        // si aggiorna la bitmap settando ad 1 i blocchi padre e figli del blocco appena preso
        update_child(&alloc->bitmap, idx, 1);
        update_parent(&alloc->bitmap, idx, 1);
        int block_size = alloc->min_bucket_size << (alloc->num_levels - level);
        char *ret = alloc->memory + ((startIdx(idx) + 1) * block_size);

        // ci salviamo nel blocco l'indice corrispettivo della bitmap
        // essendoci fatto spazio precedentemente
        ((int *)ret)[0] = idx;
        ((int *)ret)[1] = size;
        return (void *)(ret + 2 * sizeof(int)); // + size dell'indirizzo del blocco in bitmap
    }
}

void *BuddyAllocator_malloc(BuddyAllocator *alloc, int size)
{
    if (size < 0)
    {
        printf("[ERRORE] Dimensione non valida per malloc.\n");
        return NULL;
    }
    if (size == 0)
    {
        printf("[ERRORE] Impossibile allocare 0 byte.\n");
        return NULL;
    }

    int org_size = size;
    size += 2 * sizeof(int); // aggiungo spazio per salvare metadata (indice del blocco e dimensione) nella bitmap

    if (size > alloc->memory_size)
    {
        printf("[ERRORE] Memoria richiesta (%d bytes) supera la memoria disponibile (%d bytes).\n", size, alloc->memory_size);
        return NULL;
    }

    // we determine the level of the page
    int mem_size = (1 << alloc->num_levels) * alloc->min_bucket_size;
    // log2(mem_size): n bits to represent the whole memory
    // log2(size): n nits to represent the requested chunk
    // bits_mem_size-bits_size = depth of the chunk = level
    int level = floor(log2(mem_size / size));
    // if the level is too small, we pad it to max
    if (level > alloc->num_levels)
    {
        level = alloc->num_levels;
    }

    printf("[INFO] Allocazione richiesta: %d bytes (+%lu bytes overhead), livello: %d\n",
           org_size, 2 * sizeof(int), level);

    void *indirizzo = BuddyAllocator_getBuddy(alloc, level, org_size);
    if (indirizzo == NULL)
    {
        printf("[ERRORE] Nessun blocco di memoria libero disponibile.\n");
    }
    else
    {
        printf("[SUCCESSO] Allocazione completata: indirizzo %p, dimensione %d bytes\n", indirizzo, alloc->min_bucket_size << (alloc->num_levels - level));
        return indirizzo;
    }
    return NULL;
}

void BuddyAllocator_free(BuddyAllocator *alloc, void *mem)
{
    if (!mem)
    {
        printf("[ERRORE] Puntatore NULL fornito a free.\n");
        return;
    }

    int *p = (int *)mem;
    p--;
    int idx = *(--p);
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
        return;

    int value = BitMap_bit(bitmap, bit);
    if (value == 1)
        return;

    int buddy = buddyIdx(bit);
    value = BitMap_bit(bitmap, buddy);
    if (value == 1)
        return;

    int parent = parentIdx(bit);
    BitMap_setBit(bitmap, parent, 0);
    merge(bitmap, parent);
}

void update_parent(BitMap *bitmap, int bit, int value)
{
    BitMap_setBit(bitmap, bit, value);
    if (bit > 0)
        update_parent(bitmap, parentIdx(bit), value);
}

void update_child(BitMap *bitmap, int bit, int value)
{
    if (bit < bitmap->num_bits)
    {
        BitMap_setBit(bitmap, bit, value);
        // ricorsione sui figli, in discesa
        update_child(bitmap, bit * 2 + 1, value);
        update_child(bitmap, bit * 2 + 2, value);
    }
}
