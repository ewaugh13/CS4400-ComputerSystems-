#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <elf.h>
#include "decode.h"

/* Given the in-memory ELF header pointer as `ehdr` and a section
   header pointer as `shdr`, returns a pointer to the memory that
   contains the in-memory content of the section */
#define AT_SEC(ehdr, shdr) ((void *)(ehdr) + (shdr)->sh_offset)

static int get_secrecy_level(const char *s); /* foward declaration */

/*************************************************************/

int get_section_index(Elf64_Ehdr* ehdr, Elf64_Shdr* shdrs, void* strs, char* section_name)
{
  int i;
  for (i = 0; i < ehdr->e_shnum; i++) 
  {
     if(strcmp(shdrs[i].sh_name+strs - 1, section_name) == 0)
     {
        return i;
     }
  }
}

Elf64_Addr get_next_instruction(Elf64_Addr code_address, instruction_t current_instruction)
{
    return code_address += current_instruction.length;
}

char* search_rela(Elf64_Rela *relas, int size_rela, Elf64_Sym *dynsyms, void* strs, Elf64_Addr target_address)
{
  int i;
  for (i = 0; i < size_rela; i++)
  {
    if(relas[i].r_offset == target_address)
    {
      int sym_index = ELF64_R_SYM(relas[i].r_info);
      char* name = strs + dynsyms[sym_index].st_name;
      return name;
    }
  }
  return "";
}

void decode_function_instructions(int current_secrecy_level, int current_inst_length, int inst_length, Elf64_Rela *rela_plts, int size_rela_plt, Elf64_Rela *rela_dyns, int size_rela_dyn,
   Elf64_Sym *dynsyms, void* strs, Elf64_Ehdr *ehdr, Elf64_Addr inst_address, Elf64_Addr inst_runtime_address)
{
    instruction_t current_instruction;
    decode(&current_instruction, (code_t*)inst_address, inst_runtime_address);
    current_inst_length += current_instruction.length;

    if(current_instruction.op == MOV_ADDR_TO_REG_OP)
    {
        char* global_name = search_rela(rela_dyns, size_rela_dyn, dynsyms, strs, current_instruction.mov_addr_to_reg.addr);
        if(global_name != "")
            if(get_secrecy_level(global_name) > current_secrecy_level)
                replace_with_crash((code_t*)inst_address, &current_instruction);
    }
    else if(current_instruction.op == JMP_TO_ADDR_OP || current_instruction.op == MAYBE_JMP_TO_ADDR_OP) //have to look at what we are jumping to and its secrecy level
    {
        instruction_t jump_instruction;
        decode(&jump_instruction, (code_t*)(void*)ehdr + current_instruction.jmp_to_addr.addr, current_instruction.jmp_to_addr.addr);
        char* jump_name = search_rela(rela_plts, size_rela_plt, dynsyms, strs, jump_instruction.jmp_to_addr.addr);
        if(jump_name != "")
        {
            if(get_secrecy_level(jump_name) > current_secrecy_level)
            {
                replace_with_crash((code_t*)inst_address, &current_instruction);
            }
        }
    }
    inst_address = get_next_instruction(inst_address, current_instruction);
    inst_runtime_address = get_next_instruction(inst_runtime_address, current_instruction);
    if(current_inst_length < inst_length)
      decode_function_instructions(current_secrecy_level, current_inst_length, inst_length, rela_plts, size_rela_plt, rela_dyns, size_rela_dyn, dynsyms, strs, ehdr, inst_address, inst_runtime_address);
}

void redact(Elf64_Ehdr *ehdr) {
  /* Your work starts here --- but add helper functions that you call
     from here, instead of trying to put all the work in one
     function. */
   
     Elf64_Shdr *shdrs = (void*)ehdr + ehdr->e_shoff;
     void* strs = AT_SEC(ehdr, shdrs + ehdr->e_shstrndx);
     ++strs;

     int index_dynsym = get_section_index(ehdr, shdrs, strs, ".dynsym");
     int index_dyn_str = get_section_index(ehdr, shdrs, strs, ".dynstr");

     int index_rela_dyn = get_section_index(ehdr, shdrs, strs, ".rela.dyn"); //gets info for variable slike a0 and a1
     int index_rela_plt = get_section_index(ehdr, shdrs, strs, ".rela.plt"); //gets info for variables like a0 and a1 also

     Elf64_Rela *rela_plts = AT_SEC(ehdr, shdrs + index_rela_plt);
     int rela_plt_count = shdrs[index_rela_plt].sh_size / sizeof(Elf64_Rela);

     Elf64_Rela *rela_dyns = AT_SEC(ehdr, shdrs + index_rela_dyn);
     int rela_dyn_count = shdrs[index_rela_dyn].sh_size / sizeof(Elf64_Rela);

     Elf64_Sym *dynsyms = AT_SEC(ehdr, shdrs + index_dynsym);
     void* dyn_strs = AT_SEC(ehdr, shdrs + index_dyn_str);
     int size_symbols = shdrs[index_dynsym].sh_size;
     int size_dyn_sym = size_symbols/sizeof(Elf64_Sym);
     int i;
     for (i = 0; i < size_dyn_sym; i++)
     {
       // 1 is obj, 2 is func
        if((ELF64_ST_TYPE(dynsyms[i].st_info) == 1 || ELF64_ST_TYPE(dynsyms[i].st_info) == 2) && dynsyms[i].st_size > 0)
        {
          char* name = dyn_strs + dynsyms[i].st_name;
          int type = ELF64_ST_TYPE(dynsyms[i].st_info);
          int size_instructions = dynsyms[i].st_size;
          if(type == 2)
          {            
              Elf64_Addr inst_address = (void*)ehdr + dynsyms[i].st_value;
              Elf64_Addr inst_runtime_address = dynsyms[i].st_value;
              int current_secrecy_level = get_secrecy_level(name);
              decode_function_instructions(current_secrecy_level, 0, size_instructions, rela_plts, rela_plt_count, rela_dyns, rela_dyn_count, dynsyms, dyn_strs, ehdr, inst_address, inst_runtime_address);
          }
        }
      }  
}

/*************************************************************/

static int get_secrecy_level(const char *s) {
  int level = 0;
  int len = strlen(s);
  while (len && (s[len-1] >= '0') && (s[len-1] <= '9')) {
    level = (level * 10) + (s[len-1] - '0');
    --len;
  }
  return level;
}

static void fail(char *reason, int err_code) {
  fprintf(stderr, "%s (%d)\n", reason, err_code);
  exit(1);
}

static void check_for_shared_object(Elf64_Ehdr *ehdr) {
  if ((ehdr->e_ident[EI_MAG0] != ELFMAG0)
      || (ehdr->e_ident[EI_MAG1] != ELFMAG1)
      || (ehdr->e_ident[EI_MAG2] != ELFMAG2)
      || (ehdr->e_ident[EI_MAG3] != ELFMAG3))
    fail("not an ELF file", 0);

  if (ehdr->e_ident[EI_CLASS] != ELFCLASS64)
    fail("not a 64-bit ELF file", 0);
  
  if (ehdr->e_type != ET_DYN)
    fail("not a shared-object file", 0);
}

int main(int argc, char **argv) {
  int fd_in, fd_out;
  size_t len;
  void *p_in, *p_out;
  Elf64_Ehdr *ehdr;

  if (argc != 3)
    fail("expected two file names on the command line", 0);

  /* Open the shared-library input file */
  fd_in = open(argv[1], O_RDONLY);
  if (fd_in == -1)
    fail("could not open input file", errno);

  /* Find out how big the input file is: */
  len = lseek(fd_in, 0, SEEK_END);

  /* Map the input file into memory: */
  p_in = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd_in, 0);
  if (p_in == (void*)-1)
    fail("mmap input failed", errno);

  /* Since the ELF file starts with an ELF header, the in-memory image
     can be cast to a `Elf64_Ehdr *` to inspect it: */
  ehdr = (Elf64_Ehdr *)p_in;

  /* Check that we have the right kind of file: */
  check_for_shared_object(ehdr);

  /* Open the shared-library output file and set its size: */
  fd_out = open(argv[2], O_RDWR | O_CREAT, 0777);
  if (fd_out == -1)
    fail("could not open output file", errno);
  if (ftruncate(fd_out, len))
    fail("could not set output file file", errno);

  /* Map the output file into memory: */
  p_out = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_out, 0);
  if (p_out == (void*)-1)
    fail("mmap output failed", errno);

  /* Copy input file to output file: */
  memcpy(p_out, p_in, len);

  /* Close input */
  if (munmap(p_in, len))
    fail("munmap input failed", errno);    
  if (close(fd_in))
    fail("close input failed", errno);

  ehdr = (Elf64_Ehdr *)p_out;

  redact(ehdr);

  if (munmap(p_out, len))
    fail("munmap input failed", errno);    
  if (close(fd_out))
    fail("close input failed", errno);

  return 0;
}
