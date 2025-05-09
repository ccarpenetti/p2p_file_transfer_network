#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "index.h"

// Global index
int n = 1;

int get_index(char *dir, int len, FILE *output){

    struct dirent *d_ptr;
    struct stat file_stat;

    DIR *base_dir = opendir(dir);

    int num_lines = 0;

    // Check that we successfully opened the directory
    if (base_dir == NULL){
        printf("Error: %s\n", strerror(errno));
        return -1;
    }

    // Loop through all files, starting at the base directory
    while ((d_ptr = readdir(base_dir)) != NULL){

        // Ignore printing base directory or parent directory
        if ((strcmp(d_ptr->d_name, ".") == 0) || (strcmp(d_ptr->d_name, "..") == 0)){
            continue;
        }
        
        // Create current absolute path
        char path[BUFSIZ];
        sprintf(path, "%s/%s", dir, d_ptr->d_name);

        if (d_ptr->d_type == DT_DIR){
            // If a directory, recurse
            num_lines += get_index(path, len, output);
        }
        else {

            //Get size of file
            if (stat(path, &file_stat) == -1){
                printf("Error: %s\n", strerror(errno));
                fclose(output);
                closedir(base_dir);
                return -1;
            }
            
            // Print out relative path only
            char *relative_path;
            relative_path = path;
            relative_path += len + 1;

            struct tm *mod_date = localtime(&file_stat.st_mtime); //Include last modified date

            fprintf(output, "%d;%s;%d;%d-%d-%d\n", n++, relative_path, file_stat.st_size, mod_date->tm_year + 1900, mod_date->tm_mon + 1, mod_date->tm_mday);
            num_lines++;
        
        }

    }

    closedir(base_dir);
    return num_lines;
}
