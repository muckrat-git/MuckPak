/* Muckpak by .muckrat */

#ifndef MUCKPAK_H
#define MUCKPAK_H

/* Possible defines : */
/* MUCKPAK_CREATE_ARCHIVE - Can create archives from folders */
/*                          Optional as it requires several OS specific functions */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define MUCKPAK_FOLDER

#ifdef MUCKPAK_CREATE_ARCHIVE
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Ensure filesystem register defines exist
#ifndef DT_DIR
#define DT_DIR 4
#define DT_REG 8
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#endif

#define M_FILE_BASE_SIZE (1+sizeof(long)+sizeof(long))
// File structure for storing file information
typedef struct m_file {
    uint8_t name_size;      // File name size
    char * name;            // Filename
    unsigned long size;     // File size
    unsigned long offset;   // File offset in the archive
} m_file;

#define M_FOLDER_BASE_SIZE (1+4+4)
// Folder structure for storing folder information
typedef struct m_folder {
    uint8_t name_size;  // Length of name text
    char * name;        // Folder name

    // File and folder counts
    unsigned int file_count;
    unsigned int folder_count;

    m_file * files;
    m_folder * subfolders;
} m_folder;

// Ambiguous entry type for files and folders
typedef struct m_entry {
    bool exists;        // True if the entry exists
    bool is_file;       // True if this entry is a file, false if it's a folder
    m_file * file;
    m_folder * folder;
} m_entry;

#define M_PACKAGE_HEAD_SIZE (4+8+8) // ID + struct size + data size

// Unarchived package structure
typedef struct package {
    char id[4];                 // Optional Package ID (4 bytes)

    // Size information
    unsigned long struct_size;  // Size of the package structure
    unsigned long data_size;    // Size of the data in the package

    m_folder root;              // Root folder
    uint8_t * data;             // Data for all files
} package;

// Raw binary archive data (use package to access contents)
typedef struct archive {
    unsigned long size;
    uint8_t * data;
} archive;

// - Package creation functions -

#ifdef MUCKPAK_CREATE_ARCHIVE

// Load a folder structure recursively
m_folder _load_folder(const char * folder_path, const char * folder_name, unsigned long * offset, package * pkg) {
    m_folder folder = {};
    folder.name_size = strlen(folder_name);
    folder.name = (char *)malloc(folder.name_size + 1);
    strcpy(folder.name, folder_name);
    folder.file_count = 0;
    folder.folder_count = 0;

    // Increment package structure size
    pkg->struct_size += M_FOLDER_BASE_SIZE + folder.name_size;

    #ifndef _WIN32
    // Open the directory
    DIR * dir = opendir(folder_path);
    if(dir == NULL) {
        perror("Failed to open directory");
        free(folder.name);
        return folder;
    }

    // Count files and subfolders
    struct dirent * entry;
    while((entry = readdir(dir)) != NULL) {
        // Skip current and parent directory entries
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        if(entry->d_type == DT_DIR)
            folder.folder_count++;
        else if(entry->d_type == DT_REG)
            folder.file_count++;
    }

    // Allocate memory for files and subfolders
    folder.files = (m_file *)malloc(sizeof(m_file) * folder.file_count);
    folder.subfolders = (m_folder *)malloc(sizeof(m_folder) * folder.folder_count);

    // Reset counts
    folder.file_count = 0;
    folder.folder_count = 0;

    // Rewind directory to read entries again
    rewinddir(dir);
    while((entry = readdir(dir)) != NULL) {
        // Skip current and parent directory entries
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full path
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", folder_path, entry->d_name);

        if(entry->d_type == DT_DIR) {
            folder.subfolders[folder.folder_count] = _load_folder(path, entry->d_name, offset, pkg);
            folder.folder_count++;
        }
        else if(entry->d_type == DT_REG) {
            m_file * file = &folder.files[folder.file_count];
            file->name_size = strlen(entry->d_name);
            file->name = (char *)malloc(file->name_size + 1);
            strcpy(file->name, entry->d_name);

            // Load the file data
            FILE * f = fopen(path, "rb");
            fseek(f, 0L, SEEK_END);
            file->size = ftell(f);
            fseek(f, 0L, SEEK_SET);

            // Add file data to the package
            pkg->data = (uint8_t *)realloc(pkg->data, *offset + file->size);
            file->offset = *offset;
            fread(pkg->data + *offset, 1, file->size, f);
            *offset += file->size;
            fclose(f);

            // Increment package structure size
            pkg->struct_size += M_FILE_BASE_SIZE + file->name_size;

            folder.file_count++;
        }
    }
    closedir(dir);
    #else

    // Windows specific code to load folder structure
    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", folder_path);

    hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to open directory: %s\n", folder_path);
        free(folder.name);
        return folder;
    }

    // First pass: count files and subfolders
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
            continue;
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            folder.folder_count++;
        else
            folder.file_count++;
    } while (FindNextFileA(hFind, &find_data));
    FindClose(hFind);

    // Allocate memory for files and subfolders
    folder.files = (m_file *)malloc(sizeof(m_file) * folder.file_count);
    folder.subfolders = (m_folder *)malloc(sizeof(m_folder) * folder.folder_count);

    // Second pass: fill files and subfolders
    folder.file_count = 0;
    folder.folder_count = 0;
    hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to open directory: %s\n", folder_path);
        free(folder.name);
        return folder;
    }
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
            continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s\\%s", folder_path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            folder.subfolders[folder.folder_count] = _load_folder(path, find_data.cFileName, offset, pkg);
            folder.folder_count++;
        } else {
            m_file * file = &folder.files[folder.file_count];
            file->name_size = strlen(find_data.cFileName);
            file->name = (char *)malloc(file->name_size + 1);
            strcpy(file->name, find_data.cFileName);

            FILE * f = fopen(path, "rb");
            fseek(f, 0L, SEEK_END);
            file->size = ftell(f);
            fseek(f, 0L, SEEK_SET);

            pkg->data = (uint8_t *)realloc(pkg->data, *offset + file->size);
            file->offset = *offset;
            fread(pkg->data + *offset, 1, file->size, f);
            *offset += file->size;
            fclose(f);

            pkg->struct_size += M_FILE_BASE_SIZE + file->name_size;

            folder.file_count++;
        }
    } while (FindNextFileA(hFind, &find_data));
    FindClose(hFind);

    #endif

    return folder;
}

// Create a package from a local folder
package load_package_folder(const char * folder) {
    package pkg = {};
    strncpy(pkg.id, "MPAK", 4);     // Set default ID
    pkg.struct_size = M_PACKAGE_HEAD_SIZE;

    // Load the folder structure
    unsigned long offset = 0;
    pkg.root = _load_folder(folder, folder, &offset, &pkg);
    pkg.data_size = offset;
    
    return pkg;
}

// Save a folder structure to a local directory
void _save_folder(package pkg, m_folder folder, const char * path) {
    // Create the folder if it doesn't exist
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, folder.name);
    
    #ifdef _WIN32
    if(mkdir(full_path)) {
    #else
    if(mkdir(full_path, 0777)) {
    #endif
        perror("Failed to create directory");
        return;
    }
    
    // Recursively save subfolders
    for(unsigned int i = 0; i < folder.folder_count; ++i)
        _save_folder(pkg, folder.subfolders[i], full_path);
    
    // Save files
    for(unsigned int i = 0; i < folder.file_count; ++i) {
        m_file * file = &folder.files[i];
        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "%s/%s", full_path, file->name);
        FILE * f = fopen(file_path, "wb");
        fwrite(pkg.data + file->offset, 1, file->size, f);
        fclose(f);
    }
}

// Save a package unarchived to a local folder
void save_package_folder(package pkg, const char * path) {
    // Create output folder if set
    if(path[0] == '\0') path = "."; // Default to current directory
    #ifdef _WIN32
    else if(mkdir(path) && errno != EEXIST) {
        perror("Failed to create output directory");
        return;
    }
    #else
    else if(mkdir(path, 0777) && errno != EEXIST) {
        perror("Failed to create output directory");
        return;
    }
    #endif
    
    _save_folder(pkg, pkg.root, path);
}

// Save a package unarchived to to local folder
void save_package_folder(package pkg) {
    save_package_folder(pkg, ""); // Default to current directory
}

#endif

// Archive a folder structure into a single data binary
uint8_t * _archive_folder(m_folder folder, uint8_t * data) {
    // Archive folder name
    data[0] = folder.name_size;
    memcpy(data + 1, folder.name, folder.name_size);
    data += folder.name_size + 1;

    // Folder content counts
    memcpy(data, &folder.file_count, 4);
    memcpy(data + 4, &folder.folder_count, 4);
    data += 4 + 4;

    // Copy files
    for(unsigned int i = 0; i < folder.file_count; ++i) {
        m_file file = folder.files[i];
        data[0] = file.name_size;
        memcpy(data + 1, file.name, file.name_size);
        data += 1 + file.name_size;
        memcpy(data, &file.size, sizeof(file.size));
        data += sizeof(file.size);
        memcpy(data, &file.offset, sizeof(file.offset));
        data += sizeof(file.offset);
    }

    // Copy subfolders
    for(unsigned int i = 0; i < folder.folder_count; ++i)
        data = _archive_folder(folder.subfolders[i], data);
    return data; // Return updated data pointer
}

// Archive a package into a single data binary
archive archive_package(package pkg) {
    archive arc = {};
    arc.size = pkg.struct_size + pkg.data_size;
    arc.data = (uint8_t *)malloc(arc.size);

    // Write package header
    unsigned long offset = 0;
    memcpy(arc.data + offset, pkg.id, 4);
    offset += 4;
    memcpy(arc.data + offset, &pkg.struct_size, 8);
    offset += 8;
    memcpy(arc.data + offset, &pkg.data_size, 8);
    offset += 8;

    // Write folder structure
    _archive_folder(pkg.root, arc.data + offset);
    offset = pkg.struct_size;

    // Write file data
    memcpy(arc.data + offset, pkg.data, pkg.data_size);

    return arc;
}

// Unarchive a folder from an archive data
m_folder _unarchive_folder(uint8_t ** data) {
    m_folder folder = {};

    // Load folder name
    folder.name_size = *data[0];
    (*data)++;
    folder.name = (char *)malloc(folder.name_size + 1);
    memcpy(folder.name, *data, folder.name_size);
    folder.name[folder.name_size] = '\0';
    *data += folder.name_size;

    // Load file and folder counts
    memcpy(&folder.file_count, (*data), 4);
    memcpy(&folder.folder_count, (*data) + 4, 4);
    *data += 4 + 4;

    // Allocate memory for files and subfolders
    folder.files = (m_file *)malloc(sizeof(m_file) * folder.file_count);
    folder.subfolders = (m_folder *)malloc(sizeof(m_folder) * folder.folder_count);

    // Read files
    for(unsigned int i = 0; i < folder.file_count; ++i) {
        m_file * file = &folder.files[i];
        file->name_size = (*data)[0];
        file->name = (char *)malloc(file->name_size + 1);
        memcpy(file->name, (*data) + 1, file->name_size);
        file->name[file->name_size] = '\0';
        (*data) += 1 + file->name_size;

        memcpy(&file->size, (*data), sizeof(file->size));
        (*data) += sizeof(file->size);
        memcpy(&file->offset, (*data), sizeof(file->offset));
        (*data) += sizeof(file->offset);
    }

    // Read subfolders
    for(unsigned int i = 0; i < folder.folder_count; ++i)
        folder.subfolders[i] = _unarchive_folder(data);

    return folder;
}

// Unarchive a package from an archive
package unarchive_package(archive arc) {
    package pkg = {};
    memcpy(pkg.id, arc.data, 4);
    pkg.struct_size = *(unsigned long *)(arc.data + 4);
    pkg.data_size = *(unsigned long *)(arc.data + 12);
    
    // Allocate memory for data
    pkg.data = (uint8_t *)malloc(pkg.data_size);
    memcpy(pkg.data, arc.data + pkg.struct_size, pkg.data_size);
    
    // Unarchive the root folder
    arc.data += M_PACKAGE_HEAD_SIZE;
    pkg.root = _unarchive_folder(&arc.data);

    return pkg;
}

// Save an archive to a file
void save_archive(const char * filename, archive arc) {
    FILE * f = fopen(filename, "wb");
    if(f) {
        fwrite(arc.data, 1, arc.size, f);
        fclose(f);
    } else {
        perror("Failed to save archive");
    }
}

// Save a package to an archive file
void save_package_to_archive(const char * filename, package pkg) {
    archive arc = archive_package(pkg);
    save_archive(filename, arc);
    free(arc.data); // Free the archive data after saving
}

// Load an archive from a file
archive load_archive(const char * filename) {
    archive arc = {};
    FILE * f = fopen(filename, "rb");
    if(f) {
        fseek(f, 0L, SEEK_END);
        arc.size = ftell(f);
        fseek(f, 0L, SEEK_SET);
        arc.data = (uint8_t *)malloc(arc.size);
        fread(arc.data, 1, arc.size, f);
        fclose(f);
    } else {
        perror("Failed to load archive");
    }
    return arc;
}

// Load a package from an archive file
package load_package(const char * filename) {
    archive arc = load_archive(filename);
    if(arc.data) {
        package pkg = unarchive_package(arc);
        free(arc.data); // Free the archive data after unarchiving
        return pkg;
    } else {
        package empty_pkg = {};
        return empty_pkg; // Return an empty package if loading failed
    }
}

// - Package reading functions - 

// Get a file/folder entry in a specific folder
m_entry get_entry_in_folder(m_folder folder, const char * name) {
    m_entry entry = {};
    entry.exists = true;
    
    // Check files
    for(unsigned int i = 0; i < folder.file_count; ++i) {
        if(strcmp(folder.files[i].name, name) == 0) {
            entry.is_file = true;
            entry.file = &folder.files[i];
            return entry;
        }
    }

    // Check subfolders
    for(unsigned int i = 0; i < folder.folder_count; ++i) {
        if(strcmp(folder.subfolders[i].name, name) == 0) {
            entry.is_file = false;
            entry.folder = &folder.subfolders[i];
            return entry;
        }
    }

    entry.exists = false; // Not found
    return entry;
}

// Get a file reference by path
m_file * get_file(package pkg, const char * path) {
    // Split the path into components
    char * path_copy = strdup(path);
    char * token = strtok(path_copy, "/");
    m_folder * current_folder = &pkg.root;

    while(token != NULL) {
        m_entry entry = get_entry_in_folder(*current_folder, token);
        if(!entry.exists) {
            free(path_copy);
            return NULL; // Not found
        }

        if(entry.is_file) {
            free(path_copy);
            return entry.file; // Found file
        } else if(entry.folder) {
            current_folder = entry.folder; // Move to subfolder
        } else {
            free(path_copy);
            return NULL; // Not found
        }
        token = strtok(NULL, "/");
    }

    free(path_copy);
    return NULL; // Not found
}

// Get a file's binary data
uint8_t * get_file_binary(m_file file, package pkg) {
    return pkg.data + file.offset; // Return pointer to file data in package
}

// Read a file's content as text (must be freed)
char * read_file_text(m_file file, package pkg) {
    char * content = (char *)malloc(file.size + 1);
    memcpy(content, get_file_binary(file, pkg), file.size);
    content[file.size] = '\0';
    return content;
}

void _free_folder(m_folder folder) {
    // Free files
    for(unsigned int i = 0; i < folder.file_count; ++i)
        free(folder.files[i].name);
    free(folder.files);

    // Free subfolders
    for(unsigned int i = 0; i < folder.folder_count; ++i) {
        _free_folder(folder.subfolders[i]);
    }
    free(folder.subfolders);

    // Free folder name
    free(folder.name);
}

// Free a package
void free_package(package pkg) {
    _free_folder(pkg.root);
    free(pkg.data);
}

// Dump a directory structure to stdout
void dump_directory(m_folder folder, const char * prefix) {
    printf("%s%s/\n", prefix, folder.name);

    // Create padding for sub folders/files
    size_t prefix_len = strlen(prefix);
    char * padding = (char *)malloc(prefix_len + 5);
    strcpy(padding, prefix);
    strcat(padding, "    ");

    // Dump files
    for(unsigned int i = 0; i < folder.file_count; ++i)
        printf("%s%s\n", padding, folder.files[i].name);

    // Dump folders
    for(unsigned int i = 0; i < folder.folder_count; ++i)
        dump_directory(folder.subfolders[i], padding);
    
    free(padding);
}

// - Raylib integration functions -
#ifdef RAYLIB_H

// Load an image from a file in the package
Image m_load_image(package pkg, m_file file) {
    // Get file extension 
    const char * ext = strrchr(file.name, '.');
    if(ext == NULL) {
        TraceLog(LOG_WARNING, "File has no extension: %s", file.name);
        return {};
    }

    return LoadImageFromMemory(ext, get_file_binary(file, pkg), file.size);
}

// Load an image from a path in the package
Image m_load_image(package pkg, const char * path) {
    m_file * file = get_file(pkg, path);
    if(file) {
        return m_load_image(pkg, *file);
    } 
    else {
        TraceLog(LOG_WARNING, "File not found: %s", path);
        return {};
    }
}

// Load a texture from a file in the package
Texture2D m_load_texture(package pkg, m_file file) {
    Image img = m_load_image(pkg, file);
    if(img.data) {
        Texture2D texture = LoadTextureFromImage(img);
        UnloadImage(img); // Free image data after loading texture
        return texture;
    } 
    else {
        TraceLog(LOG_WARNING, "Failed to load image from file: %s", file.name);
        return {};
    }
}

// Load a texture from a file in the package
Texture2D m_load_texture(package pkg, const char * path) {
    m_file * file = get_file(pkg, path);
    if(file) {
        return m_load_texture(pkg, *file);
    } 
    else {
        TraceLog(LOG_WARNING, "File not found: %s", path);
        return {};
    }
}

#endif

#endif