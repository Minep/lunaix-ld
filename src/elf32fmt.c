#include <unistd.h>
#include <fcntl.h>
#include <lunaix/mann.h>
#include <errno.h>

#include "elf32.h"
#include "ldmalloc.h"
#include "helper.h"

#define ALIGN(v, a) (((v) + (a)) & ~(a))

int
elf32_check_arch(const struct elf32* elf)
{
    const struct elf32_ehdr* ehdr = &elf->eheader;

    return *(uint32_t*)(ehdr->e_ident) == ELFMAGIC &&
           ehdr->e_ident[EI_CLASS] == ELFCLASS32 &&
           ehdr->e_ident[EI_DATA] == ELFDATA2LSB && ehdr->e_machine == EM_386;
}

struct elf32* elf32_open(const char* path) {
    int fd = open(path, O_RDWR);

    if (fd <= 0) {
        return NULL;
    }

    struct elf32* elffile = ldmalloc(sizeof(struct elf32));

    if (!elffile) {
        return NULL;
    }

    elffile->fd = fd;

    int erno = read(fd, &elffile->eheader, sizeof(elffile->eheader));

    if (erno <= 0 || !elf32_check_arch(elffile)) {
        goto err;
    }

    size_t phdr_sz = elffile->eheader.e_phentsize * elffile->eheader.e_phnum;
    elffile->pheaders = ldmalloc(phdr_sz);

    lseek(fd, elffile->eheader.e_phoff, FSEEK_SET);

    if (read(fd, elffile->pheaders, phdr_sz) <= 0) {
        goto err;
    }

    for (size_t i = 0; i < elffile->eheader.e_phnum; i++)
    {
        struct elf32_phdr* phdr = &elffile->pheaders[i];
        if (phdr->p_type == PT_DYNAMIC) {
            elffile->dynamic = phdr->p_va;
        }

        if (phdr->p_type == PT_LOAD) {
            size_t align = phdr->p_align - 1;
            elffile->load_size += (phdr->p_memsz + align) & ~align;
        }
    }

    return elffile;

err:
    close(fd);
    return NULL;
}

int elf32_load(struct elf32* elffile) {
    uintptr_t base = 0;
    int err = 0;
    int map_options = MAP_FIXED_NOREPLACE;

    if (elffile->eheader.e_type == ET_DYN) {
        base = mmap(0, elffile->load_size, PROT_WRITE, MAP_ANON, 0, 0);
        if (!base) {
            return geterrno();
        }
        map_options = MAP_FIXED;
    }

    elffile->dynamic += base;
    elffile->eheader.e_entry += base;
    
    for (size_t i = 0; i < elffile->eheader.e_phnum; i++)
    {
        struct elf32_phdr* phdr = &elffile->pheaders[i];
        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        size_t align = phdr->p_align - 1;
        uintptr_t va = phdr->p_va + base;
        size_t memsz = ALIGN(phdr->p_memsz, align);
        size_t filesz = phdr->p_filesz;
        int prot = 0;

        if ((phdr->p_flags & PF_W)) {
            prot |= PROT_WRITE;
        }
        else if ((phdr->p_flags & PF_R)) {
            prot |= PROT_READ;
        }
        else if ((phdr->p_flags & PF_X)) {
            prot |= PROT_EXEC;
        }

        size_t sz = memsz - filesz;
        if (!(sz & ~align)) {
            // last page in mmaped region contains the last file page of that
            err = mmap(va, memsz, prot, map_options | MAP_SHARED, elffile->fd, phdr->p_offset);
            assert(!err, "unable to load");
            continue;
        }

        // if the gap is significant (e.g., bss section), split them, to avoid unnecessary file i/o
        mmap(va, filesz, prot, map_options | MAP_SHARED, elffile->fd, phdr->p_offset);
        mmap(va + filesz, sz & ~align, prot, map_options | MAP_SHARED | MAP_ANON, 0, 0);
    }
}