#include <string>
#include <memory.h>
#include <stdint.h>
#include <fstream>
#include <functional>
#include <iostream>

namespace Muckrat {
    // Define logging function
    #ifdef RAYLIB_H
    #define Log(msg) TraceLog(LOG_ALL, msg.c_str());
    #elseif defined(NO_STDOUT)
    #define Log(msg) (void)msg
    #else
    // Log to c out
    #define Log(msg) std::cout << (msg) << "\n"
    #endif

    // A String class limited to length 256
    class ShortString {
        public:
        uint8_t * content;  // The raw bytes for the string

        // Get the length of the string
        inline uint8_t length() {
            return content[0];
        }
        // Get the string a char pointer array
        inline char * chars() {
            return (char*)(content+1);
        }
        // Get as a C string
        inline const char * cStr() {
            return (const char *)(content+1);
        }
        
        /* - Cast operator overloads - */
        operator char*()        { return this->chars(); }
        operator const char *() { return this->cStr(); }
        operator std::string () {
            return std::string(this->cStr(), (size_t)length());
        }

        /* Equals operator */
        bool operator == (ShortString rhs) {
            if(this->length() != rhs.length()) return false;

            // Check each char
            for(uint8_t i = 0; i < this->length(); ++i) {
                if(content[i+1] != rhs.content[i+1]) return false;
            }
            return true;
        }
        bool operator != (ShortString rhs) {
            return !(*this == rhs);
        }
    };

    // Package file
    class File {
        public:
        ShortString name;           // Filename
        uint8_t * data = nullptr;   // Raw binary data of the file
        unsigned long size;         // Size of the file's raw binary data

        File() = default;

        // - File reading functions -

        // Get file data as text
        std::string getText() {
            return std::string((const char*)data, (size_t)size);
        }
    };

    // Package folder
    class Folder {
        public:
        ShortString name;   // Folder name
        
        uint32_t fileCount; 
        File * files;

        uint32_t folderCount;
        Folder * folders;

        // Get file in immediate folder (null if not found)
        File * getFile(std::string filename) {
            for(uint32_t i = 0; i < fileCount; ++i) {
                if((std::string)files[i].name == filename) 
                    return &files[i];
            }
            return nullptr;
        }

        // Get subfolder in immediate folder (null if not found) 
        Folder * getFolder(std::string filename) {
            for(uint32_t i = 0; i < folderCount; ++i) {
                if((std::string)folders[i].name == filename) 
                    return &folders[i];
            }
            return nullptr;
        }

        // Free up the memory taken by the folders subelements
        void Unload() {
            if(fileCount)
                delete[] files;
        
            // Recursively unload subfolders
            for(unsigned int i = 0; i < folderCount; ++i)
                folders[i].Unload();
            if(folderCount) delete[] folders;
        }
    };
    
    class Package {
        private:
        unsigned long headerSize, dataSize;
        uint8_t * data;     // The package's raw data
        uint8_t * fileData; // Pointer to the file content section of data

        Folder _LoadFolder(uint8_t *& source) {
            Folder folder = {};

            // Load folder name
            folder.name.content = source;
            source += folder.name.length() + 1;

            // Load file/folder ocunts
            folder.fileCount = *(unsigned int *)(source);
            folder.folderCount = *(unsigned int *)(source + 4);
            source += 8;

            // Allocate files and folders
            folder.files = new File[folder.fileCount];
            folder.folders = new Folder[folder.folderCount];

            // Load files
            for(unsigned int i = 0; i < folder.fileCount; ++i) {
                File & file = folder.files[i];

                // Load file name
                file.name.content = source;
                source += file.name.length() + 1;

                // Load data information
                file.size = *(unsigned long *)(source);
                source += 8;
                unsigned long offset = *(unsigned long *)(source);
                file.data = fileData + offset;
                source += 8;
            }

            // Load subfolders
            for(unsigned int i = 0; i < folder.folderCount; ++i)
                folder.folders[i] = _LoadFolder(source);
            
            return folder;
        }

        public:
        bool loaded = false;
        char * id;      // Optional 4 character package id
        Folder root;    // The package root folder

        // Load the package from an array of bytes
        void LoadFromMemory(uint8_t * source) {
            data = source;
            id = (char*)source; // ID is first 4 bytes of data
            headerSize = *(unsigned long *)(source + 4);
            dataSize =  *(unsigned long *)(source + 12);

            // Set pointers
            fileData = data + headerSize;           // File data pointer
            uint8_t * structureData = data + 20;    // File structure pointer

            // Load root
            root = _LoadFolder(structureData);
        }

        // Get a file from a path
        File getFile(std::string path) {
            size_t index = 0;
            std::string token;
            Folder * current = &root;

            // Search through tokens
            while((index = path.find('/')) != std::string::npos) {
                token = path.substr(0, index);
                
                // Get next folder
                current = current->getFolder(token);
                if(current == nullptr) {
                    Log("Failed to find file '" + path + "'");
                    return {};
                }

                path.erase(0, index + 1);
            }

            // Find file
            File * file = current->getFile(path);
            if(current == nullptr) {
                Log("Failed to find file '" + path + "'");
                return {};
            }
            return *file;
        }

        Package(std::string filename) {
            std::fstream file(filename, std::ios::in | std::ios::binary);
            if(!file.is_open()) {
                Log("Failed to load file '" + filename + "'");
                data = nullptr;
                loaded = false;
                return;
            }

            // Load file data
            file.seekg(0, std::ios::end);
		    size_t size = file.tellg();
		    file.seekg(0, std::ios::beg);
		    data = new uint8_t[size];
		    file.read((char*)data, size);

            // Load
            LoadFromMemory(data);
            loaded = true;
        }

        // Dump the directory structure to log
        void Dump() {
            // Define lambda for recursive folder dumping
            auto _DumpFolder = [](auto self, Folder folder, std::string prefix) -> void {
                Log(prefix + (std::string)folder.name + "/");
                prefix += "    ";

                // Dump file names
                for(unsigned int i = 0; i < folder.fileCount; ++i)
                    Log(prefix + (std::string)folder.files[i].name);

                // Recursively dump folders
                for(unsigned int i = 0; i < folder.folderCount; ++i)
                    self(self, folder.folders[i], prefix);
            };

            // Dump the root
            _DumpFolder(_DumpFolder,root, "");
        }

        ~Package() {
            if(data == nullptr || !loaded) return;

            root.Unload();
            delete[] data;
        }
    };

}