#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mman.h>

#define NEW_WIDTH 600
#define NEW_HEIGHT 600
#define BLOCK_SIZE 100                      
#define NUM_BLOCKS (NEW_HEIGHT / BLOCK_SIZE)
#define TOTAL_CHILDREN (NUM_BLOCKS * NUM_BLOCKS)

void read_and_pad(const char *filename, int padded[NEW_HEIGHT][NEW_WIDTH]);
void write_image(const char *filename, int image[NEW_HEIGHT][NEW_WIDTH]);

void skip_comments(FILE *file) {
    int c;
    while ((c = fgetc(file)) != EOF) {
        if (c == '#') {
            while ((c = fgetc(file)) != '\n' && c != EOF);
        } else if (c != '\n' && c != '\r' && c != ' ' && c != '\t') {
            ungetc(c, file);
            break;
        }
    }
}

int main() {
    int padded1[NEW_HEIGHT][NEW_WIDTH] = {0};
    int padded2[NEW_HEIGHT][NEW_WIDTH] = {0};

    read_and_pad("image1.pgm", padded1);
    read_and_pad("image2.pgm", padded2);

    long long (*result)[NEW_WIDTH] = mmap(NULL, sizeof(long long[NEW_HEIGHT][NEW_WIDTH]),
                                          PROT_READ | PROT_WRITE,
                                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(result == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    
    memset(result, 0, sizeof(long long[NEW_HEIGHT][NEW_WIDTH]));
    
    pid_t pids[TOTAL_CHILDREN];
    int child_index = 0;

    for (int block_row = 0; block_row < NUM_BLOCKS; block_row++) {
        for (int block_col = 0; block_col < NUM_BLOCKS; block_col++) {
            pid_t pid = fork();
            if(pid < 0) {
                perror("fork failed");
                exit(EXIT_FAILURE);
            }
            if(pid == 0) {  
                int row_start = block_row * BLOCK_SIZE;
                int col_start = block_col * BLOCK_SIZE;
                for (int i = row_start; i < row_start + BLOCK_SIZE; i++) {
                    for (int j = col_start; j < col_start + BLOCK_SIZE; j++) {
                        long long sum = 0;
                        for (int k = 0; k < NEW_WIDTH; k++) {
                            sum += (long long) padded1[i][k] * padded2[k][j];
                        }
                        result[i][j] = sum;
                    }
                }
                exit(EXIT_SUCCESS);
            } else {
                pids[child_index++] = pid;
            }
        }
    }

    for (int i = 0; i < TOTAL_CHILDREN; i++) {
        wait(NULL);
    }

    long long max_val = 1;
    for (int i = 0; i < NEW_HEIGHT; i++) {
        for (int j = 0; j < NEW_WIDTH; j++) {
            if(result[i][j] > max_val)
                max_val = result[i][j];
        }
    }

    int output[NEW_HEIGHT][NEW_WIDTH];
    for (int i = 0; i < NEW_HEIGHT; i++) {
        for (int j = 0; j < NEW_WIDTH; j++) {
            output[i][j] = (int)((result[i][j] * 255) / max_val);
        }
    }

    write_image("output.pgm", output);
    munmap(result, sizeof(long long[NEW_HEIGHT][NEW_WIDTH]));
    printf("Matrix multiplication complete. Output saved to output.pgm\n");
    return 0;
}

void read_and_pad(const char *filename, int padded[NEW_HEIGHT][NEW_WIDTH]) {
    memset(padded, 0, sizeof(int[NEW_HEIGHT][NEW_WIDTH]));
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening image file");
        exit(EXIT_FAILURE);
    }

    char format[3];
    if (fscanf(file, "%2s", format) != 1) {
        fprintf(stderr, "Error reading image format\n");
        exit(EXIT_FAILURE);
    }
    skip_comments(file);

    int in_width, in_height, max_val;
    if(fscanf(file, "%d %d %d", &in_width, &in_height, &max_val) != 3) {
        fprintf(stderr, "Error reading dimensions or max value\n");
        exit(EXIT_FAILURE);
    }
    fgetc(file);

    int copy_width = in_width < NEW_WIDTH ? in_width : NEW_WIDTH;
    int copy_height = in_height < NEW_HEIGHT ? in_height : NEW_HEIGHT;

    if(strcmp(format, "P2") == 0) {
        for (int i = 0; i < copy_height; i++) {
            for (int j = 0; j < copy_width; j++) {
                if(fscanf(file, "%d", &padded[i][j]) != 1) {
                    fprintf(stderr, "Error reading pixel\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
    } else if(strcmp(format, "P5") == 0) {
        for (int i = 0; i < copy_height; i++) {
            if(fread(&padded[i][0], sizeof(unsigned char), copy_width, file) != (size_t)copy_width) {
                fprintf(stderr, "Error reading binary pixel\n");
                exit(EXIT_FAILURE);
            }
        }
    } else {
        fprintf(stderr, "Unsupported format: %s\n", format);
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

void write_image(const char *filename, int image[NEW_HEIGHT][NEW_WIDTH]) {
    FILE *file = fopen(filename, "w");
    if(!file) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "P2\n%d %d\n255\n", NEW_WIDTH, NEW_HEIGHT);
    for (int i = 0; i < NEW_HEIGHT; i++) {
        for (int j = 0; j < NEW_WIDTH; j++) {
            fprintf(file, "%d ", image[i][j]);
        }
        fprintf(file, "\n");
    }
    fclose(file);
}
