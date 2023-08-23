#ifndef __LD_ELF32_H
#define __LD_ELF32_H

typedef unsigned int elf32_ptr_t;
typedef unsigned short elf32_hlf_t;
typedef unsigned int elf32_off_t;
typedef unsigned int elf32_swd_t;
typedef unsigned int elf32_wrd_t;

#define ET_NONE 0
#define ET_EXEC 2
#define ET_DYN 3

#define PT_LOAD 1
#define PT_INTERP 3
#define PT_DYNAMIC 2

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

#define EM_NONE 0
#define EM_386 3

#define EV_CURRENT 1

// [0x7f, 'E', 'L', 'F']
#define ELFMAGIC 0x464c457fU
#define ELFCLASS32 1
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define EI_CLASS 4
#define EI_DATA 5

struct elf32_ehdr
{
    char e_ident[16];
    elf32_hlf_t e_type;
    elf32_hlf_t e_machine;
    elf32_wrd_t e_version;
    elf32_ptr_t e_entry;
    elf32_off_t e_phoff;
    elf32_off_t e_shoff;
    elf32_wrd_t e_flags;
    elf32_hlf_t e_ehsize;
    elf32_hlf_t e_phentsize;
    elf32_hlf_t e_phnum;
    elf32_hlf_t e_shentsize;
    elf32_hlf_t e_shnum;
    elf32_hlf_t e_shstrndx;
};

struct elf32_phdr
{
    elf32_wrd_t p_type;
    elf32_off_t p_offset;
    elf32_ptr_t p_va;
    elf32_ptr_t p_pa;
    elf32_wrd_t p_filesz;
    elf32_wrd_t p_memsz;
    elf32_wrd_t p_flags;
    elf32_wrd_t p_align;
};

struct elf32
{
    int fd;
    struct elf32_ehdr eheader;
    struct elf32_phdr* pheaders;
    struct elf32_dynent* dynamic;
    uintptr_t base;
    size_t load_size;
};

struct elf32_dynent
{
    elf32_swd_t d_tag;
    union
    {
        elf32_wrd_t d_val;
        elf32_ptr_t d_ptr;
    } d_un;
};

struct elf32_syment
{
    elf32_wrd_t st_name;
    elf32_ptr_t st_val;
    elf32_wrd_t st_size;
    char st_other;
    char st_shndx;
};

struct elf32_shdr
{
    elf32_wrd_t sh_name;
    elf32_wrd_t sh_type;
    elf32_wrd_t sh_flags;
    elf32_ptr_t sh_addr;
    elf32_off_t sh_offset;
    elf32_wrd_t sh_size;
    elf32_wrd_t sh_link;
    elf32_wrd_t sh_info;
    elf32_wrd_t sh_align;
    elf32_wrd_t sh_entsz;
};

struct elf32_file
{
    int fd;
    struct elf32_ehdr header;
};

#endif /* __LD_ELF32_H */
