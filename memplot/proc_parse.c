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
    char mem_usage[64];
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

ProcessNode* create_process_node(const char *name, int pid, int depth) {
    ProcessNode *node = malloc(sizeof(ProcessNode));
    if (!node) return NULL;
    strncpy(node->name, name, 255);
    node->pid = pid;
    node->depth = depth;
    node->collapsed = 0;
    strcpy(node->mem_usage, "N/A");
    node->cpu_usage = 0.0;
    node->child = NULL;
    node->next = NULL;
    return node;
}

void update_process_info(ProcessNode *node) {
    if (!node || node->pid <= 0) return;
    char path[256], buffer[256];
    FILE *fp;
    unsigned long utime = 0, stime = 0;

    snprintf(path, sizeof(path), "/proc/%d/status", node->pid);
    if ((fp = fopen(path, "r"))) {
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strncmp(buffer, "VmRSS:", 6) == 0) {
                sscanf(buffer, "VmRSS:%s", node->mem_usage);
                break;
            }
        }
        fclose(fp);
    }

    snprintf(path, sizeof(path), "/proc/%d/stat", node->pid);
    if ((fp = fopen(path, "r"))) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            sscanf(buffer, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu", &utime, &stime);
            node->cpu_usage = (double)(utime + stime) / sysconf(_SC_CLK_TCK);
        }
        fclose(fp);
    }
}

void render_process_tree() {
    clear();
    for (int i = scroll_offset; i < visible_count && i < scroll_offset + (LINES - 2); i++) {
        ProcessNode *node = visible_nodes[i];
        int x = node->depth * 4;
        if (i == selected_index) attron(A_REVERSE);
        mvprintw(i - scroll_offset, x, "%s %s [PID: %d] Mem: %s CPU: %.2f%%",
                 node->collapsed ? "[+]" : "[-]", node->name, node->pid, node->mem_usage, node->cpu_usage);
        if (i == selected_index) attroff(A_REVERSE);
    }
    refresh();
}

void* update_thread_func(void *arg) {
    while (1) {
        sleep(2);
        for (int i = 0; i < visible_count; i++) {
            update_process_info(visible_nodes[i]);
        }
    }
    return NULL;
}

int main() {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    pthread_t update_thread;
    pthread_create(&update_thread, NULL, update_thread_func, NULL);

    int ch;
    while ((ch = getch()) != 'q') {
        switch (ch) {
            case KEY_UP:
                if (selected_index > 0) selected_index--;
                break;
            case KEY_DOWN:
                if (selected_index < visible_count - 1) selected_index++;
                break;
            case '\n':
                if (selected_index < visible_count)
                    visible_nodes[selected_index]->collapsed = !visible_nodes[selected_index]->collapsed;
                flatten_tree();
                break;
        }
        render_process_tree();
    }

    pthread_cancel(update_thread);
    pthread_join(update_thread, NULL);
    endwin();
    return 0;
}
