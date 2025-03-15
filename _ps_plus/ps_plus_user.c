#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_VISIBLE_NODES 1024

typedef struct ProcessNode {
    char name[256];
    int pid;
    int depth;
    int collapsed;
    char details[512];      
    unsigned long prev_cpu_time; 
    double cpu_usage;       
    struct ProcessNode *child;  
    struct ProcessNode *next;  
} ProcessNode;

ProcessNode *head = NULL;

ProcessNode *visible_nodes[MAX_VISIBLE_NODES];
int visible_count = 0;
int selected_index = 0;
int scroll_offset = 0; 

void flatten_tree_recursive(ProcessNode *node) {
    if (!node) return;
    if (visible_count < MAX_VISIBLE_NODES)
        visible_nodes[visible_count++] = node;
    if (!node->collapsed)
        flatten_tree_recursive(node->child);
    flatten_tree_recursive(node->next);
}

void flatten_tree() {
    visible_count = 0;
    flatten_tree_recursive(head);
}

void add_process_node(const char *line) {
    int space_count = 0;
    while (line[space_count] == ' ') space_count++;
    int depth = space_count / 2; 

    char proc_name[256] = {0};
    int pid = -1;
    if (sscanf(line + space_count, "%255s [%d]", proc_name, &pid) != 2) {
        strncpy(proc_name, line + space_count, 255);
    }

    ProcessNode *new_node = malloc(sizeof(ProcessNode));
    if (!new_node) return;
    strncpy(new_node->name, proc_name, 255);
    new_node->pid = pid;
    new_node->depth = depth;
    new_node->collapsed = 0;
    new_node->child = NULL;
    new_node->next = NULL;
    new_node->prev_cpu_time = 0;
    new_node->cpu_usage = 0.0;
    snprintf(new_node->details, sizeof(new_node->details),
             "Process: %s (PID: %d)\nMemory Usage: N/A\nCPU Usage: N/A", new_node->name, new_node->pid);

    static ProcessNode *parent_stack[50] = {NULL};
    if (depth == 0) {
        new_node->next = head;
        head = new_node;
    } else {
        ProcessNode *parent = parent_stack[depth - 1];
        if (parent) {
            if (!parent->child) {
                parent->child = new_node;
            } else {
                ProcessNode *sibling = parent->child;
                while (sibling->next)
                    sibling = sibling->next;
                sibling->next = new_node;
            }
        } else {
            new_node->next = head;
            head = new_node;
        }
    }
    parent_stack[depth] = new_node;
}

void free_process_nodes(ProcessNode *node) {
    if (!node) return;
    free_process_nodes(node->child);
    free_process_nodes(node->next);
    free(node);
}

unsigned long long get_global_cpu_time() {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 0;
    char buffer[1024];
    if (!fgets(buffer, sizeof(buffer), fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    if (sscanf(buffer, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) < 8)
        return 0;
    return user + nice + system + idle + iowait + irq + softirq + steal;
}

void update_details_recursive(ProcessNode *node, unsigned long long global_delta) {
    if (!node) return;
    if (node->pid > 0) {
        char path[256];
        char mem[64] = "N/A";
        unsigned long utime = 0, stime = 0;
        snprintf(path, sizeof(path), "/proc/%d/status", node->pid);
        FILE *fp = fopen(path, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "VmRSS:", 6) == 0) {
                    sscanf(line, "VmRSS:%63s", mem);
                    break;
                }
            }
            fclose(fp);
        }
        snprintf(path, sizeof(path), "/proc/%d/stat", node->pid);
        fp = fopen(path, "r");
        unsigned long current_proc_time = 0;
        if (fp) {
            char buffer[1024];
            if (fgets(buffer, sizeof(buffer), fp)) {
                char comm[256];
                if (sscanf(buffer,
                           "%*d (%[^)]) %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu",
                           comm, &utime, &stime) == 3) {
                    current_proc_time = utime + stime;
                }
            }
            fclose(fp);
        }
        if (node->prev_cpu_time == 0) {
            node->cpu_usage = 0.0;
        } else {
            unsigned long delta_proc = current_proc_time - node->prev_cpu_time;
            node->cpu_usage = (global_delta > 0) ? ((double)delta_proc / (double)global_delta * 100.0) : 0.0;
        }
        node->prev_cpu_time = current_proc_time;
        snprintf(node->details, sizeof(node->details),
                 "Process: %s (PID: %d)\nMemory Usage: %s\nCPU Usage: %.2f%%",
                 node->name, node->pid, mem, node->cpu_usage);
    }
    update_details_recursive(node->child, global_delta);
    update_details_recursive(node->next, global_delta);
}

void* update_thread_func(void *arg) {
    unsigned long long global_cpu_prev = get_global_cpu_time();
    while (1) {
        sleep(2);
        unsigned long long global_cpu_now = get_global_cpu_time();
        unsigned long long global_delta = (global_cpu_now > global_cpu_prev) ? (global_cpu_now - global_cpu_prev) : 1;
        global_cpu_prev = global_cpu_now;
        update_details_recursive(head, global_delta);
    }
    return NULL;
}

void load_process_tree() {
    FILE *file = fopen("/proc/process_tree", "r");
    if (!file) {
        perror("Failed to open /proc/process_tree");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        add_process_node(line);
    }
    fclose(file);
}

void render_visible_tree() {
    int max_rows = LINES - 4; 
    for (int i = scroll_offset; i < visible_count && i < scroll_offset + max_rows; i++) {
        ProcessNode *node = visible_nodes[i];
        int x = 2 + node->depth * 4;
        int y = i - scroll_offset;
        if (i == selected_index)
            attron(A_REVERSE);
        mvprintw(y, x, "%s %s", node->collapsed ? "[+]" : "[-]", node->name);
        if (i == selected_index)
            attroff(A_REVERSE);
    }
}

void render_details(const char *details, int start_row) {
    char details_copy[512];
    strncpy(details_copy, details, sizeof(details_copy));
    details_copy[sizeof(details_copy)-1] = '\0';
    char *line = strtok(details_copy, "\n");
    int row = start_row;
    while (line != NULL && row < LINES) {
        mvprintw(row++, 2, "%s", line);
        line = strtok(NULL, "\n");
    }
}

int main() {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    load_process_tree();
    flatten_tree();

    selected_index = 0;
    scroll_offset = 0;

    pthread_t update_thread;
    if (pthread_create(&update_thread, NULL, update_thread_func, NULL)) {
        endwin();
        fprintf(stderr, "Error creating update thread\n");
        exit(EXIT_FAILURE);
    }

    int ch;
    while ((ch = getch()) != 'q') {
        int max_rows = LINES - 4;
        switch (ch) {
            case KEY_UP:
                if (selected_index > 0)
                    selected_index--;
                break;
            case KEY_DOWN:
                if (selected_index < visible_count - 1)
                    selected_index++;
                break;
            case '\n': 
                if (selected_index < visible_count)
                    visible_nodes[selected_index]->collapsed = !visible_nodes[selected_index]->collapsed;
                break;
            default:
                break;
        }

        flatten_tree();
        if (selected_index < scroll_offset)
            scroll_offset = selected_index;
        else if (selected_index >= scroll_offset + max_rows)
            scroll_offset = selected_index - max_rows + 1;

        clear();
        render_visible_tree();
        if (selected_index < visible_count) {
            render_details(visible_nodes[selected_index]->details, max_rows + 1);
        }
        refresh();
    }

    pthread_cancel(update_thread);
    pthread_join(update_thread, NULL);
    endwin();
    free_process_nodes(head);
    return 0;
}
