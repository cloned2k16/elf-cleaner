#include <algorithm>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdarg.h>

#include "elf.h"                                                                                                        // Include a local elf.h copy as not all platforms have it.

#define DT_VERSYM           0x6ffffff0
#define DT_FLAGS_1          0x6ffffffb
#define DT_VERNEEDED        0x6ffffffe
#define DT_VERNEEDNUM       0x6fffffff

#define DF_1_NOW            0x00000001                                                                                  //  Set RTLD_NOW for this object.
#define DF_1_GLOBAL         0x00000002                                                                                  //  Set RTLD_GLOBAL for this object.  
#define DF_1_NODELETE       0x00000008                                                                                  //  Set RTLD_NODELETE for this object.

                                                                                                                        // The supported DT_FLAGS_1 values as of Android 6.0.
#define SUPPORTED_DT_FLAGS_1 (DF_1_NOW | DF_1_GLOBAL | DF_1_NODELETE)

#define MAX_FMT_STR_LEN                     254                                                                                             // we don't send larger lines here !

#define ERROR_CANT_OPEN_FILE                101     
#define ERROR_CANT_GET_FILE_PERMISSIONS     102     
#define ERROR_FAIL_TO_MEMMAP                103
#define ERROR_PROCESSING_ELF32              104
#define ERROR_PROCESSING_ELF64              105
#define ERROR_BAD_EFI_CLASS                 106
#define FATAL_ERROR_SYNC_MEM                107
//  --------------------------- ----------------------------------- ----------------------------------------------------
//  --------------------------- ----------------------------------- ----------------------------------------------------    
    char                        buff                                [MAX_FMT_STR_LEN+2];
//  --------------------------- ----------------------------------- ----------------------------------------------------
    void                       _out                                 (FILE* strm,const char* fmt, va_list args)          {
        int  len = strlen(fmt);
             len = len<MAX_FMT_STR_LEN?
                   len:MAX_FMT_STR_LEN;
             
        strncpy(buff,fmt,len);                                                                                          // better safe than sorry!
        buff[len]='\n';
    
        vfprintf    (strm   , buff,args);
    }
//  --------------------------- ----------------------------------- ----------------------------------------------------
    void                       _err                                 (const char* fmt,...)                               {
        va_list      args;
        va_start    (args   , fmt);
        _out        (stderr , fmt,args);
        va_end      (args);
    }
//  --------------------------- ----------------------------------- ----------------------------------------------------
    void                       _dbg                                 (const char* fmt,...)                               {
        va_list      args;
        va_start    (args   , fmt);
        _out        (stdout , fmt,args);
        va_end      (args);
    }
//  --------------------------- ----------------------------------- ----------------------------------------------------        
//  --------------------------- ----------------------------------- ----------------------------------------------------        
    template<   typename ElfHeaderType                                                                                  //  Elf{32,64}_Ehdr
            ,   typename ElfSectionHeaderType                                                                           //  Elf{32,64}_Shdr
            ,   typename ElfDynamicSectionEntryType                                                                     //  Elf{32,64}_Dyn 
            >
    bool                        process_elf                         (uint8_t* bytes, size_t elf_file_size
                                                                    , char const* file_name)                            {
        if (sizeof(ElfSectionHeaderType) > elf_file_size) {
            _dbg("Elf header for '%s' would end at %zu but file size only %zu", file_name, (long)sizeof(ElfSectionHeaderType), (long)elf_file_size);
            return false;
        }
        ElfHeaderType*          elf_hdr                 = reinterpret_cast<ElfHeaderType*>(bytes);

        size_t last_section_header_byte = elf_hdr->e_shoff + sizeof(ElfSectionHeaderType) * elf_hdr->e_shnum;
        if (last_section_header_byte > elf_file_size) {
            _dbg("Section header for '%s' would end at %zu but file size only %zu", file_name, last_section_header_byte, elf_file_size);
            return false;
        }
        ElfSectionHeaderType*   section_header_table    = reinterpret_cast<ElfSectionHeaderType*>(bytes + elf_hdr->e_shoff);

        for (unsigned int i = 1; i < elf_hdr->e_shnum; i++) {
            ElfSectionHeaderType* section_header_entry = section_header_table + i;
            if (section_header_entry->sh_type == SHT_DYNAMIC) {
                size_t const last_dynamic_section_byte = section_header_entry->sh_offset + section_header_entry->sh_size;
                if (last_dynamic_section_byte > elf_file_size) {
                    _dbg("Dynamic section for '%s' would end at %zu but file size only %zu", file_name, last_dynamic_section_byte, elf_file_size);
                    return false;
                }

                size_t const dynamic_section_entries = section_header_entry->sh_size / sizeof(ElfDynamicSectionEntryType);
                ElfDynamicSectionEntryType* const dynamic_section = reinterpret_cast<ElfDynamicSectionEntryType*>(bytes + section_header_entry->sh_offset);

                unsigned int last_nonnull_entry_idx = 0;
                for (unsigned int j = dynamic_section_entries - 1; j > 0; j--) {
                    ElfDynamicSectionEntryType* dynamic_section_entry = dynamic_section + j;
                    if (dynamic_section_entry->d_tag != DT_NULL) {
                        last_nonnull_entry_idx = j;
                        break;
                    }
                }

                for (unsigned int j = 0; j < dynamic_section_entries; j++) {
                    ElfDynamicSectionEntryType* dynamic_section_entry = dynamic_section + j;
                    char const* removed_name = nullptr;
                    switch (dynamic_section_entry->d_tag) {
                        case DT_VERSYM: removed_name        = "DT_VERSYM";      break;
                        case DT_VERNEEDED: removed_name     = "DT_VERNEEDED";   break;
                        case DT_VERNEEDNUM: removed_name    = "DT_VERNEEDNUM";  break;
                        case DT_VERDEF: removed_name        = "DT_VERDEF";      break;
                        case DT_VERDEFNUM: removed_name     = "DT_VERDEFNUM";   break;
                        case DT_RPATH: removed_name         = "DT_RPATH";       break;
                        case DT_RUNPATH: removed_name       = "DT_RUNPATH";     break;
                    }
                    if (removed_name != nullptr) {
                        _dbg("elf-cleaner: Removing the %s dynamic section entry from '%s'", removed_name, file_name);
        // Tag the entry with DT_NULL and put it last:
                        dynamic_section_entry->d_tag = DT_NULL;
                    
        // Decrease j to process new entry index:
                        std::swap(dynamic_section[j--], dynamic_section[last_nonnull_entry_idx--]);
                    } else if (dynamic_section_entry->d_tag == DT_FLAGS_1) {
        // Remove unsupported DF_1_* flags to avoid linker warnings.
                        decltype(dynamic_section_entry->d_un.d_val) orig_d_val  =   dynamic_section_entry->d_un.d_val;
                        decltype(dynamic_section_entry->d_un.d_val) new_d_val   =   (orig_d_val & SUPPORTED_DT_FLAGS_1);
                        if (new_d_val != orig_d_val) {
                            _dbg("Replacing unsupported DF_1_* flags %llu with %llu in '%s'",
                                (unsigned long long) orig_d_val,
                                (unsigned long long) new_d_val,
                                file_name);
                            dynamic_section_entry->d_un.d_val = new_d_val;
                        }
                    }
                }
            } 
            else if (   section_header_entry->sh_type == SHT_GNU_verdef 
                    ||  section_header_entry->sh_type == SHT_GNU_verneed 
                    ||  section_header_entry->sh_type == SHT_GNU_versym) {
                        _dbg("Removing version section from '%s'", file_name);
                        section_header_entry->sh_type = SHT_NULL;
            }
        }
        return true;
    }
//  --------------------------- ----------------------------------- ----------------------------------------------------
    int                         usage                               (int argc, const char** argv)                       {
        int err =argc < 2 || strcmp(argv[1], "-h")==0;
        if ( err ) {
            _err("usage: ");
            _err("       %s <filename> [[filename] [filename] ..]", argv[0]);
            _err("\n         removes unsupported section types from ELF files,"
                 "\n           which the Android linker use to complain about."
                 "\n"
                 );
        }
        return err;
    }
//  --------------------------- ----------------------------------- ----------------------------------------------------
    int                         main                                (int argc, const char** argv)                       {
        int err=usage (argc,argv);

        if (!err)
        for (int i = 1; i < argc; i++) {
            char const* file_name = argv[i];
            int  fd = open(file_name, O_RDWR);                                                                              // we better leave intact the original !!
            if (fd < 0) {
                _err("can't open file: '%s'",file_name);
                return ERROR_CANT_OPEN_FILE;
            }

            struct stat st;
            if (fstat(fd, &st) < 0) { 
                _err("ca't get file permissions ! (%s)",file_name); 
                return ERROR_CANT_GET_FILE_PERMISSIONS; 
            }

            if (st.st_size < (long long) sizeof(Elf32_Ehdr)) {
                close(fd);
                _dbg("skiping: '%s' ( file too small )",file_name);
                continue;
            }

            void* mem = mmap(0, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (mem == MAP_FAILED) { 
                _err("mapping in memory: '%s'",file_name); 
                return ERROR_FAIL_TO_MEMMAP; 
            }

            uint8_t* bytes = reinterpret_cast<uint8_t*>(mem);
            if (!( bytes[0] == 0x7F 
                && bytes[1] == 'E' 
                && bytes[2] == 'L' 
                && bytes[3] == 'F')) {
                    _dbg("skiping: '%s' ( wrong ELF magic )",file_name);
                    munmap(mem, st.st_size);
                    close(fd);
                    continue;
            }

            if (bytes[5 /*EI_DATA*/] != 1) {
                _err("skipping: '%s' not in little endian", file_name);
                munmap(mem, st.st_size);
                close(fd);
                continue;
            }

            uint8_t const bit_value = bytes[4   /*EI_CLASS*/];
            if      (bit_value == 1) {
                if (!process_elf<Elf32_Ehdr, Elf32_Shdr, Elf32_Dyn>(bytes, st.st_size, file_name)) {
                    munmap(mem, st.st_size);
                    close(fd);
                    return ERROR_PROCESSING_ELF32;
                }
            } 
            else if (bit_value == 2) {
                if (!process_elf<Elf64_Ehdr, Elf64_Shdr, Elf64_Dyn>(bytes, st.st_size, file_name)) {
                    munmap(mem, st.st_size);
                    close(fd);
                    return ERROR_PROCESSING_ELF64;
                }
            } 
            else {
                _err("Incorrect EI_CLASS value %d in '%s'\n", bit_value, file_name);
                munmap(mem, st.st_size);
                close(fd);
                return ERROR_BAD_EFI_CLASS;
            }

            if (msync(mem, st.st_size, MS_SYNC) < 0) { 
                _err("'%s' error MS_SYNC",file_name); 
                munmap(mem, st.st_size);
                close(fd);
                return FATAL_ERROR_SYNC_MEM; 
            }

            munmap(mem, st.st_size);
            close(fd);
        }
        return err;
    }
//  --------------------------- ----------------------------------- ----------------------------------------------------
