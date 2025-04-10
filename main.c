#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>

void error_and_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int launch_stage(const char *cmd, char *const argv[], int input_fd, int *output_fd) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) error_and_exit("pipe");

    pid_t pid = fork();
    if (pid == -1) error_and_exit("fork");

    if (pid == 0) {
        if (input_fd != -1) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        execvp(cmd, argv);
        error_and_exit(cmd);
    }

    if (input_fd != -1) close(input_fd);
    close(pipe_fd[1]);
    *output_fd = pipe_fd[0];
    return pid;
}

int run_grep(int input_fd, int *output_fd) {
    char *args[] = {"grep", "-oP", "(?<= - - \\[)[0-9]{2}/[A-Za-z]{3}/[0-9]{4}", NULL};
    return launch_stage("grep", args, input_fd, output_fd);
}

int run_tr(int input_fd, int *output_fd) {
    char *args[] = {"tr", "-d", "/", NULL};
    return launch_stage("tr", args, input_fd, output_fd);
}

int run_date_format(int input_fd, int *output_fd) {
    char *args[] = {"xargs", "-I{}", "date", "-d", "{}", "+%Y-%m-%d", NULL};
    return launch_stage("xargs", args, input_fd, output_fd);
}

int run_sort(int input_fd, int *output_fd) {
    char *args[] = {"sort", NULL};
    return launch_stage("sort", args, input_fd, output_fd);
}

int run_uniq_count(int input_fd, int *output_fd) {
    char *args[] = {"uniq", "-c", NULL};
    return launch_stage("uniq", args, input_fd, output_fd);
}

int run_sort_count_desc(int input_fd, int *output_fd) {
    char *args[] = {"sort", "-k1", "-nr", NULL};
    return launch_stage("sort", args, input_fd, output_fd);
}

int run_head10(int input_fd, int *output_fd) {
    char *args[] = {"head", "-n", "10", NULL};
    return launch_stage("head", args, input_fd, output_fd);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <log_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Open log file
    int input_fd = open(argv[1], O_RDONLY);
    if (input_fd == -1) error_and_exit("open");

    int out_fd;

    pid_t pids[10]; int pid_count = 0;

    pids[pid_count++] = run_grep(input_fd, &out_fd);
    pids[pid_count++] = run_tr(out_fd, &out_fd);
    pids[pid_count++] = run_date_format(out_fd, &out_fd);
    pids[pid_count++] = run_sort(out_fd, &out_fd);
    pids[pid_count++] = run_uniq_count(out_fd, &out_fd);
    pids[pid_count++] = run_sort_count_desc(out_fd, &out_fd);
    pids[pid_count++] = run_head10(out_fd, &out_fd);

    FILE *final = fdopen(out_fd, "r");
    if (!final) error_and_exit("fdopen");

    char *line = NULL;
    size_t len = 0;
    int total_hits = 0, hits[10] = {0};
    char dates[10][64];
    int count = 0;

    while(getline(&line, &len, final) != -1 && count < 10) {
        char *token = strtok(line, " \t\n");
        if (!token) continue;

        hits[count] = (int) strtol(token, NULL, 10);

        token = strtok(NULL, " \t\n");
        if (!token) continue;

        strncpy(dates[count], token, sizeof(dates[count]) - 1);
        dates[count][sizeof(dates[count]) - 1] = '\0';

        total_hits += hits[count];
        count++;
    }

    fclose(final);
    free(line);

    for (int i = 0; i < count; ++i) {
        int percent = hits[i] * 100 / total_hits;
        printf("%d. %s - %d - %d%%\n", i + 1, dates[i], hits[i], percent);
    }

    // wait for all commands we are running
    for (int i = 0; i < pid_count; ++i) waitpid(pids[i], NULL, 0);

    // measuring execution time
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("\nЧас виконання: %.2f секунд\n", duration);

    return 0;
}
