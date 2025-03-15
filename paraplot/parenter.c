#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define NUM_CHILDREN 5  
void write_parent_pid_to_proc() {
    int fd = open("/proc/child_tree", O_WRONLY);
    if (fd < 0) {
        perror("Error opening /proc/child_tree");
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", getpid()); 
    write(fd, buf, strlen(buf));
    close(fd);
}

void print_tree_format(const char *buffer) {
    int indent_level = 0;
    const char *ptr = buffer;

    while (*ptr) {
        if (*ptr == '\n') {
            printf("\n");
        } else if (*ptr == '|') {
            printf("\n");
            for (int i = 0; i < indent_level; i++)
                printf("  "); 
            printf("|-");
        } else if (*ptr == 'P' && strncmp(ptr, "Parent", 6) == 0) {
            indent_level = 0; 
            printf("\n");
        } else if (*ptr == 'C' && strncmp(ptr, "Child", 5) == 0) {
            indent_level += 2; 
        }
        putchar(*ptr);
        ptr++;
    }
    printf("\n");
}


int main() {
    write_parent_pid_to_proc();  

    pid_t pids[NUM_CHILDREN];

    printf("Parent PID: %d\n", getpid());

    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {  
            printf("Child PID: %d, Parent PID: %d\n", getpid(), getppid());
            sleep(3);  
            exit(EXIT_SUCCESS);
        } else {
            pids[i] = pid;
        }
    }

    sleep(2);

    char buffer[8192];
    int fd = open("/proc/child_tree", O_RDONLY);
    if (fd < 0) {
        perror("Error opening /proc/child_tree");
        return 1;
    }
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    if (bytesRead < 0) {
        perror("Error reading /proc/child_tree");
        close(fd);
        return 1;
    }
    buffer[bytesRead] = '\0'; 
    close(fd);

    printf("\nProcess Tree:\n");
    print_tree_format(buffer);

    for (int i = 0; i < NUM_CHILDREN; i++) {
        wait(NULL);
    }

    return 0;
}
