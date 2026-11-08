# WRC Evo Tools
A set of tools for files found in WRC games developed by Evolution Studios on the PS2 platform.

## Features
Currently, the tool can:
- Extract the contents of `DVD.DAT` and assign filenames to the extracted files based on the listed hash in `DVD.BIN` and the currently known filenames listed in `src/filenames.hpp`
- Hash filenames and look up names from hashes
- Build new `DVD.DAT` and `DVD.BIN` files from a input directory
- Import a single file back into `DVD.DAT` (if not larger than the original)
- Rebuild ELF into decrypted form from a PCSX2 `eeMemory.bin` savestate

Planned features include:
- Solving the encryption/compression algorithm found in EVO container files.
- Full reimporting and rebuilding support for EVO data files.

## Usage

```shell
.\evo_tools [cmd] [file|text]
```

### Modes

- **-hash [text]**: Hash provided text using the custom hash found in the games.
- **-name [hash]**: Get filename from hash value.
- **-udvd [DVD.BIN] [DVD.DAT] [out dir]**: Extract DVD.DAT to a specified output directory.
- **-rdvd [DVD.BIN] [DVD.DAT] [input dir]**: Build new DVD files from a directory.
- **-idvd [DVD.BIN] [DVD.DAT] [input file]**: Reimport a file (cannot be bigger than original).
- **-eelf [eeMemory.bin] [out ELF] [--packed ELF]**: Rebuild a decrypted ELF from a PCSX2 savestate. Any of `--dest`, `--size`, `--entry` and `--memsz` can override the values read from `--packed`; `--packed-vaddr`, `--scratchpad-memsz` and `--eflags` are also supported.

### Examples

Extracting `DVD.DAT` file contents to the `out` directory:
```shell
.\evo_tools -udvd DVD.BIN DVD.DAT .\out
```

Hash a filename:
```shell
.\evo_tools -hash somefile.dat
```

Get filename from hash:
```shell
.\evo_tools -name 12345678
```

Build DVD files from a directory:
```shell
.\evo_tools -rdvd DVD.BIN DVD.DAT .\input_dir
```

Import a single file back into DVD.DAT:
```shell
.\evo_tools -idvd DVD.BIN DVD.DAT somefile.dat
```

Rebuild a decrypted ELF from a savestate:
```shell
.\evo_tools -eelf .\savestates\wrc_5_pal_savestate\eeMemory.bin .\SCES_532.47_decrypted.ELF --packed ".\WRC Rally Evolved\SCES_532.47" --scratchpad-memsz 0x3F91
```

> **Savestate tip:** Breakpoint at `0x00100008`, that's the entry point the ELF jumps to after the decryption process is done.

## Game compatibility table

|                     **Name**                      | **Serial** | **Extract DVD.DAT/BIN** | **Build DVD.DAT/BIN** | **Import to DVD.DAT/BIN** |
|:-------------------------------------------------:|:----------:|:-------------------:|:-----------------:|:----------------:|
| WRC II Extreme                                    | SCES-50934 |  :heavy_check_mark: | :question:       | :question:       |
| WRC II Extreme (Japan)                            | SLPM 65284 |  :heavy_check_mark: | :question:       | :question:       |
| WRC 3                                             | SCES-51684 |  :heavy_check_mark: | :question:       | :question:       |
| WRC 3 (Japan)                                     | SLPM 65583 |  :heavy_check_mark: | :question:       | :question:       |
| WRC 3 (OPS2M Demo)                                | SCEE-52092 |  :heavy_check_mark: | :question:       | :question:       |
| WRC 4 1.01                                        | SCES-52389 |  :heavy_check_mark: | :question:       | :question:       |
| WRC 4 2.00                                        | SCES-52389 |  :heavy_check_mark: | :question:       | :question:       |
| WRC 4 2.00 (Japan)                                | SLPM 65975 |  :heavy_check_mark: | :question:       | :question:       |
| WRC: Rally Evolved                                | SCES-53247 |  :heavy_check_mark: | :question:       | :question:       |
| WRC avec Sébastien Loeb Édition 2005              | SCES-53680 |  :heavy_check_mark: | :question:       | :question:       |

## Building

```shell
g++ -o evo_tools src/main.cpp
```