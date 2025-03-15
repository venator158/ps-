#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>

#define MAX_TASKS 100
#define FIFO_PATH "task_fifo"

typedef struct {
    char task[256];
    int priority; // 1 = high, 2 = medium, 3 = low
} Task;

// --- Shared Memory Setup ---
// We allocate shared memory to hold an int (task count) followed by MAX_TASKS Tasks.
int shmid;
int *p_task_count;    // pointer to task count (in shared mem)
Task *task_array;     // pointer to task array (in shared mem)

// --- Executor globals (Process D) ---
pthread_mutex_t exec_mutex = PTHREAD_MUTEX_INITIALIZER;
int current_index = 0;  // index of the next task to execute

// --- Function to sort tasks by priority (ascending: 1 high, then 2, then 3) ---
void sort_tasks(Task *tasks, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (tasks[j].priority > tasks[j+1].priority) {
                Task temp = tasks[j];
                tasks[j] = tasks[j+1];
                tasks[j+1] = temp;
            }
        }
    }
}

// --- Thread function for executing tasks ---
void *executor_thread(void *arg) {
    int fifo_fd = *(int *)arg; // FIFO file descriptor for logging
    while (1) {
        pthread_mutex_lock(&exec_mutex);
        if (current_index >= *p_task_count) {
            pthread_mutex_unlock(&exec_mutex);
            break;
        }
        Task t = task_array[current_index];
        current_index++;
        pthread_mutex_unlock(&exec_mutex);
        
        // "Execute" the task (simulate execution)
        printf("[Executor] Executing: %s (Priority %d)\n", t.task, t.priority);
        sleep(1); // simulate execution delay
        
        // Write a log message to FIFO
        char log_msg[300];
        snprintf(log_msg, sizeof(log_msg), "Completed: %s (Priority %d)\n", t.task, t.priority);
        write(fifo_fd, log_msg, strlen(log_msg));
    }
    return NULL;
}

int main() {
    // --- Create FIFO for logging ---
    unlink(FIFO_PATH);  // remove if exists
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        perror("mkfifo");
        exit(1);
    }
    
    // --- Create shared memory ---
    size_t shm_size = sizeof(int) + MAX_TASKS * sizeof(Task);
    shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }
    void *shm_ptr = shmat(shmid, NULL, 0);
    if (shm_ptr == (void*)-1) {
        perror("shmat");
        exit(1);
    }
    // In shared memory, the first bytes store the task count, then the tasks array.
    p_task_count = (int *)shm_ptr;
    *p_task_count = 0;
    task_array = (Task *)((char *)shm_ptr + sizeof(int));
    
    // --- Create a pipe for Process A -> Process B communication ---
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }
    
    // --- Process A: Task Input Handler ---
    pid_t pidA = fork();
    if (pidA == 0) {
        // Child Process A
        close(pipefd[0]);  // close read end
        int num_tasks;
        printf("Enter the number of tasks: ");
        scanf("%d", &num_tasks);
        for (int i = 0; i < num_tasks; i++) {
            Task t;
            printf("Task %d (enter task name): ", i + 1);
            scanf("%s", t.task);
            printf("Priority (1=High, 2=Medium, 3=Low): ");
            scanf("%d", &t.priority);
            // Write the task structure to the pipe
            write(pipefd[1], &t, sizeof(Task));
        }
        // Send termination signal: a task with task name "DONE"
        Task done;
        strcpy(done.task, "DONE");
        done.priority = 0;
        write(pipefd[1], &done, sizeof(Task));
        close(pipefd[1]);
        printf("[Input Handler] Task input completed. Exiting...\n");
        exit(0);
    }
    
    // --- Process B: Task Queue Manager ---
    pid_t pidB = fork();
    if (pidB == 0) {
        // Child Process B
        close(pipefd[1]);  // close write end
        Task t;
        while (read(pipefd[0], &t, sizeof(Task)) == sizeof(Task)) {
            if (strcmp(t.task, "DONE") == 0) {
                break;
            }
            // Store task in shared memory (if there is room)
            if (*p_task_count < MAX_TASKS) {
                task_array[*p_task_count] = t;
                (*p_task_count)++;
            }
        }
        close(pipefd[0]);
        // Sort the tasks by priority
        sort_tasks(task_array, *p_task_count);
        printf("[Queue Manager] Task queue finalized. Exiting...\n");
        exit(0);
    }
    
    // --- Process C: Task Logger ---
    pid_t pidC = fork();
    if (pidC == 0) {
        // Child Process C
        int fifo_fd = open(FIFO_PATH, O_RDONLY);
        if (fifo_fd == -1) {
            perror("open FIFO for reading");
            exit(1);
        }
        char buffer[300];
        ssize_t n;
        while ((n = read(fifo_fd, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[n] = '\0';
            printf("[Logger] %s", buffer);
        }
        close(fifo_fd);
        exit(0);
    }
    
    // Parent waits for Process A and Process B to finish (tasks are ready)
    waitpid(pidA, NULL, 0);
    waitpid(pidB, NULL, 0);
    
    // --- Process D: Multi-threaded Task Executor ---
    pid_t pidD = fork();
    if (pidD == 0) {
        // Child Process D
        // Reattach shared memory (it is inherited by fork, but to be safe we update pointers)
        void *shm_ptr_d = shmat(shmid, NULL, 0);
        if (shm_ptr_d == (void*)-1) {
            perror("shmat in executor");
            exit(1);
        }
        p_task_count = (int *)shm_ptr_d;
        task_array = (Task *)((char *)shm_ptr_d + sizeof(int));
        
        // Open FIFO for writing log messages
        int fifo_fd = open(FIFO_PATH, O_WRONLY);
        if (fifo_fd == -1) {
            perror("open FIFO for writing");
            exit(1);
        }
        
        int num_threads = 3; // Number of worker threads
        pthread_t threads[num_threads];
        for (int i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], NULL, executor_thread, &fifo_fd);
        }
        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }
        close(fifo_fd);
        printf("[Executor] All tasks executed. Exiting...\n");
        exit(0);
    }
    
    // --- Parent waits for Processes C and D ---
    waitpid(pidC, NULL, 0);
    waitpid(pidD, NULL, 0);
    
    // Cleanup shared memory and FIFO
    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
    unlink(FIFO_PATH);
    
    return 0;
}
