# Elf-cleaner
Utility for Android ELF files to remove unused parts that the linker warns about.

# Description
When loading ELF files, 
  Android linker warns about unsupported dynamic section entries with warnings such as:

    WARNING: linker: /data/data/org.kost.nmap.android.networkmapper/bin/nmap: unused DT entry: type 0x6ffffffe arg 0x8a7d4
    WARNING: linker: /data/data/org.kost.nmap.android.networkmapper/bin/nmap: unused DT entry: type 0x6fffffff arg 0x3

This utility strips away the following dynamic section entries:

- `DT_RPATH` - not supported in any Android version.
- `DT_RUNPATH` - supported from Android 7.0.
- `DT_VERDEF` - supported from Android 6.0.
- `DT_VERDEFNUM` - supported from Android 6.0.
- `DT_VERNEEDED` - supported from Android 6.0.
- `DT_VERNEEDNUM` - supported from Android 6.0.
- `DT_VERSYM` - supported from Android 6.0.

It also removes the three ELF sections of type:

- `SHT_GNU_verdef`
- `SHT_GNU_verneed`
- `SHT_GNU_versym`

# Usage
```sh
usage: 
       ./elf-cleaner <filenames> [[filename] [filename] ..]

         removes unsupported section types from ELF files,
           which the Android linker use to complain about.
```

# Based on termux-elf-cleaner by:
   Fredrik Fornwall ([@fornwall](https://github.com/fornwall)).

   
