/* Command line tool for creating packages with muckpak */

#define MUCKPAK_CREATE_ARCHIVE
#include <muckpak.h>

int main(int argc, char * argv[]) {
    if(argc != 2 && argc != 3) {
        printf("argc: %d\n", argc);

        fprintf(stderr, "Usage:\t%s <folder_path>\n", argv[0]);
        fprintf(stderr, "      \t%s <folder_path> <tag>\n", argv[0]);
        fprintf(stderr, "      \t%s <archive_file>\n", argv[0]);
        fprintf(stderr, "      \t%s <archive_file> -d  (Dumps the archive structure)\n", argv[0]);
        return 1;
    }

    // Check if the argument is a folder or an archive file
    struct stat st;
    if(stat(argv[1], &st) == 0) {
        if(S_ISDIR(st.st_mode)) {
            // It's a folder, create a package from it
            package pkg = load_package_folder(argv[1]);

            // If a tag is provided, set it as the package ID
            if(argc == 3)
                strncpy(pkg.id, argv[2], 4);

            // Save the package to a file
            char archive_name[256];
            snprintf(archive_name, sizeof(archive_name), "%s.mpak", argv[1]);
            save_package_to_archive(archive_name, pkg);
            printf("Package created: %s\n", archive_name);

            free_package(pkg); // Free the package resources
        } 
        else if(S_ISREG(st.st_mode)) {
            // If file, dump the archive contents to the local directory
            package pkg = load_package(argv[1]);
            if(!pkg.data) {
                fprintf(stderr, "Failed to load package from %s\n", argv[1]);
                return 1;
            }
            else if(argc == 3 && strcmp(argv[2], "-d") == 0) {
                // Dump the archive structure
                printf("Package ID: %.4s\n", pkg.id);
                printf("Package Structure Size: %lu bytes\n", pkg.struct_size);
                printf("Package Data Size: %lu bytes\n", pkg.data_size);
                printf("Root Name: %s\n", pkg.root.name);
                dump_directory(pkg.root, "");
                free_package(pkg); // Free the package resources
            } 
            else if(argc == 3) {
                fprintf(stderr, "Unknown option: %s\n", argv[2]);
                free_package(pkg);
                return 1;
            }
            else {
                printf("Package ID: %.4s\n", pkg.id);
                dump_directory(pkg.root, "");

                // Save the package unarchived to the current directory
                save_package_folder(pkg, ".");
                free_package(pkg); // Free the package resources
                printf("Package unarchived in the current directory.\n");
            }

        }
    }
    else {
        fprintf(stderr, "Error: %s is not a valid folder or archive file.\n", argv[1]);
        return 1;
    }

    return 0;
}