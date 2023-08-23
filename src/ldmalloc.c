#include <ldmalloc.h>
#include <lunaix/mann.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LDMALLOC_SZ (1 << 20)

#define M_ALLOCATED 0x1
#define M_PREV_FREE 0x2

#define M_NOT_ALLOCATED 0x0
#define M_PREV_ALLOCATED 0x0

#define CHUNK_S(header) ((header) & ~0x3)
#define CHUNK_PF(header) ((header)&M_PREV_FREE)
#define CHUNK_A(header) ((header)&M_ALLOCATED)

#define PACK(size, flags) (((size) & ~0x3) | (flags))

#define SW(p, w) (*((uint32_t*)(p)) = w)
#define LW(p) (*((uint32_t*)(p)))

#define HPTR(bp) ((uint32_t*)(bp)-1)
#define BPTR(bp) ((uint8_t*)(bp) + WSIZE)
#define FPTR(hp, size) ((uint32_t*)(hp + size - WSIZE))
#define NEXT_CHK(hp) ((uint8_t*)(hp) + CHUNK_S(LW(hp)))

#define BOUNDARY 4
#define WSIZE 4

static void* ldmalloc_start;
static void* ldmalloc_end;

void
place_chunk(uint8_t* ptr, size_t size);

void*
coalesce(uint8_t* chunk_ptr);

/*
    At the beginning, we allocate an empty page and put our initial marker

    | 4/1 | 0/1 |
    ^     ^ brk
    start

    Then, expand the heap further, with HEAP_INIT_SIZE (evaluated to 4096,
    i.e.,
   1 pg size) This will allocate as much pages and override old epilogue
   marker with a free region hdr and put new epilogue marker. These are
   handled by lx_grow_heap which is internally used by alloc to expand the
   heap at many moment when needed.

    | 4/1 | 4096/0 |   .......   | 4096/0 | 0/1 |
    ^     ^ brk_old                       ^
    start                                 brk

    Note: the brk always point to the beginning of epilogue.
*/

void
ldfree(void* ptr)
{
    if (!ptr) {
        return;
    }

    uint8_t* chunk_ptr = (uint8_t*)ptr - WSIZE;
    uint32_t hdr = LW(chunk_ptr);
    size_t sz = CHUNK_S(hdr);
    uint8_t* next_hdr = chunk_ptr + sz;

    // make sure the ptr we are 'bout to free makes sense
    //   the size trick is stolen from glibc's malloc/malloc.c:4437 ;P

    SW(chunk_ptr, hdr & ~M_ALLOCATED);
    SW(FPTR(chunk_ptr, sz), hdr & ~M_ALLOCATED);
    SW(next_hdr, LW(next_hdr) | M_PREV_FREE);

    coalesce(chunk_ptr);
}

void*
lx_malloc_internal(size_t size)
{
    // Simplest first fit approach.

    if (!size) {
        return NULL;
    }

    uint8_t* ptr = ldmalloc_start;
    // round to largest 4B aligned value
    //  and space for header
    size = ROUNDUP(size + WSIZE, BOUNDARY);
    while (ptr < (uint8_t*)ldmalloc_end) {
        uint32_t header = *((uint32_t*)ptr);
        size_t chunk_size = CHUNK_S(header);
        if (!chunk_size && CHUNK_A(header)) {
            break;
        }
        if (chunk_size >= size && !CHUNK_A(header)) {
            // found!
            place_chunk(ptr, size);
            return BPTR(ptr);
        }
        ptr += chunk_size;
    }

    // Well, we are officially OOM!
    return NULL;
}

void
place_chunk(uint8_t* ptr, size_t size)
{
    uint32_t header = *((uint32_t*)ptr);
    size_t chunk_size = CHUNK_S(header);
    *((uint32_t*)ptr) = PACK(size, CHUNK_PF(header) | M_ALLOCATED);
    uint8_t* n_hdrptr = (uint8_t*)(ptr + size);
    uint32_t diff = chunk_size - size;

    if (!diff) {
        // if the current free block is fully occupied
        uint32_t n_hdr = LW(n_hdrptr);
        // notify the next block about our avaliability
        SW(n_hdrptr, n_hdr & ~0x2);
    } else {
        // if there is remaining free space left
        uint32_t remainder_hdr = PACK(diff, M_NOT_ALLOCATED | M_PREV_ALLOCATED);
        SW(n_hdrptr, remainder_hdr);
        SW(FPTR(n_hdrptr, diff), remainder_hdr);

        /*
            | xxxx |      |         |

                        |
                        v

            | xxxx |                |
        */
        coalesce(n_hdrptr);
    }
}

void*
coalesce(uint8_t* chunk_ptr)
{
    uint32_t hdr = LW(chunk_ptr);
    uint32_t pf = CHUNK_PF(hdr);
    uint32_t sz = CHUNK_S(hdr);

    uint32_t n_hdr = LW(chunk_ptr + sz);

    if (CHUNK_A(n_hdr) && pf) {
        // case 1: prev is free
        uint32_t prev_ftr = LW(chunk_ptr - WSIZE);
        size_t prev_chunk_sz = CHUNK_S(prev_ftr);
        uint32_t new_hdr = PACK(prev_chunk_sz + sz, CHUNK_PF(prev_ftr));
        SW(chunk_ptr - prev_chunk_sz, new_hdr);
        SW(FPTR(chunk_ptr, sz), new_hdr);
        chunk_ptr -= prev_chunk_sz;
    } else if (!CHUNK_A(n_hdr) && !pf) {
        // case 2: next is free
        size_t next_chunk_sz = CHUNK_S(n_hdr);
        uint32_t new_hdr = PACK(next_chunk_sz + sz, pf);
        SW(chunk_ptr, new_hdr);
        SW(FPTR(chunk_ptr, sz + next_chunk_sz), new_hdr);
    } else if (!CHUNK_A(n_hdr) && pf) {
        // case 3: both free
        uint32_t prev_ftr = LW(chunk_ptr - WSIZE);
        size_t next_chunk_sz = CHUNK_S(n_hdr);
        size_t prev_chunk_sz = CHUNK_S(prev_ftr);
        uint32_t new_hdr =
          PACK(next_chunk_sz + prev_chunk_sz + sz, CHUNK_PF(prev_ftr));
        SW(chunk_ptr - prev_chunk_sz, new_hdr);
        SW(FPTR(chunk_ptr, sz + next_chunk_sz), new_hdr);
        chunk_ptr -= prev_chunk_sz;
    }

    // (fall through) case 4: prev and next are not free
    return chunk_ptr;
}

void
ldmalloc_init()
{
    ldmalloc_start = mmap((void*)ldmalloc_init,
                          LDMALLOC_SZ,
                          PROT_WRITE | PROT_READ,
                          MAP_ANON | MAP_SHARED,
                          0,
                          0);

    if (!ldmalloc_start) {
        printf("unable to allocate buffer");
        _exit(1);
    }

    size_t sz = LDMALLOC_SZ - WSIZE;

    SW(ldmalloc_start, PACK(4, M_ALLOCATED));
    SW(ldmalloc_start + WSIZE, PACK(0, M_ALLOCATED));
    ldmalloc_end = ldmalloc_start + LDMALLOC_SZ;

    uint32_t old_marker = *((uint32_t*)ldmalloc_start);
    uint32_t free_hdr = PACK(sz, CHUNK_PF(old_marker));
    SW(ldmalloc_start, free_hdr);
    SW(FPTR(ldmalloc_start, sz), free_hdr);
    SW(NEXT_CHK(ldmalloc_start), PACK(0, M_ALLOCATED | M_PREV_FREE));

    coalesce(ldmalloc_start);
}

void*
ldmalloc(size_t sz)
{
    return lx_malloc_internal(sz);
}

void*
ldcalloc(size_t n, size_t elem)
{
    size_t pd = n * elem;

    // overflow detection
    if (pd < elem || pd < n) {
        return NULL;
    }

    void* ptr = ldmalloc(pd);
    if (!ptr) {
        return NULL;
    }

    for (size_t i = 0; i < pd; i++) {
        *((char*)ptr + i) = 0;
    }

    return ptr;
}