# MuckPak
**M**ega **U**seful **C**ontained **K**it **Packager**

MuckPak is a single header C and C++ library inspired by things like Quakes and Dooms .pak/.wad files, that provides an extremely simple interface to create portable filesystems in the form of packages (a single .mpk/.mpak/.pak file). It uses a virtual file system to allow you to keep your files organized 1 to 1 with their original source, all from a single file that can be easily distributed.

## Languages
- **muckpak.h**:    The primary interface for the muckpak format, supports all features
- **muckpak.hpp**:  C++ interface for the muckpak format, supports loading and reading pak files only (for now)

## Platforms
MuckPak is designed to be cross-platform and should work on any system (and I mean **any**, this shit runs on my calculator) that supports C and the standard library

## Tools
- **muckpak**: A command line tool to create and extract muckpak files

## How to use it

#### packages
The **package** structure represents a virtual file system or simulated directory structure, designed to be used just like your local filesystem but read only. It can be created from an **archive** or a directory, and it can be saved to an **archive** or a file.

#### archives
The **archive** structure represents the simple raw binary data of a package file. It is useless on its own but can be unarchived into a **package** or saved to a file. 

## Defines
- **MUCKPAK_CREATE_ARCHIVE**: Requires several additional includes but allows you to create and save packages from directories