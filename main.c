// Виконав Ярошенко Олександр Сергійович
// Група ІМ-21
// Варіант: 29 - 25 = 4

/*
* Виведіть списком 10 дат, відсортованих за кількістю звертань (до 10
* елементів списку, починаючи з найбільшого значення, в порядку спадання), з
* рядками у форматі <дата> - <кількість звертань в цю дату, числом> - <відсоток
* цієї кількості від загальної кількості звертань за всі ці дати>
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

// extracts date and puts it
// in external outBuffer to avoid memory leaks
// this function actually uses pipes and
// commands like grep to perform extraction
void extractDate(const char* inLine, char** outBuffer /*damn, I miss pass-by-reference from C++*/, size_t* outBufferSize);
void writeToTempFile(const char* line);



int main(int argc, char *argv[]) {

    // 1. extract file we want to use from command line
    if (argc != 2) {
        perror("Input error!\n");
        fprintf(stderr, "Usage: lab3 <file>\n");
        exit(1);
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Failed to open file");
        exit(1);
    }

    char *lineBuffer = NULL;
    size_t lineBufferSize = 0;

    char *dateBuffer = NULL;
    size_t dateBufferSize = 0;

    // getline automatically allocates buffer for the line if current is not big enough
    while (getline(&lineBuffer, &lineBufferSize, file) != -1) {
        extractDate(lineBuffer, &dateBuffer, &dateBufferSize);

        printf("Date: %s", dateBuffer);
    }

    free(lineBuffer);
    free(dateBuffer);
    fclose(file);

    return 0;
}

void extractDate(const char* inLine, char** outBuffer /*damn, I miss pass-by-reference from C++*/, size_t* outBufferSize) {
    // pipe file descriptors for grep and trunkate

    int parentToGrepPipe[2];
    int grepToTrPipe[2];
    int trToParentPipe[2];

    // process id's for grep and tr commands
    pid_t pidGrep, pidTr;

    if (pipe(parentToGrepPipe) == -1) {
        perror("failed to create pipe [inLine out->in grep]");
        exit(1);
    }

    if (pipe(grepToTrPipe) == -1) {
        perror("failed to create pipe [grep out->in tr]");
        exit(1);
    }

    if (pipe(trToParentPipe) == -1) {
        perror("failed to create pipe [tr out->in parent]");
        exit(1);
    }

    pidGrep = fork();
    if (pidGrep == -1) {
        perror("failed to create fork for grep");
        exit(1);
    }

    if (pidGrep == 0) {
        {
            close(parentToGrepPipe[1]);

            dup2(parentToGrepPipe[0], STDIN_FILENO);
            close(parentToGrepPipe[0]);
        }

        {
            // closing both ends of unused pipe here
            close(trToParentPipe[0]); close(trToParentPipe[1]);
        }

        {
            // closing unused pipe end (read) in fork,
            // it is obligatory to avoid signaling end of file incorrectly
            close(grepToTrPipe[0]);

            // redirect output from grep to the pipe
            dup2(grepToTrPipe[1], STDOUT_FILENO);
            close(grepToTrPipe[1]);
        }

        // assembling the grep command like in bash script
        char *grepAssembly[] = {
            "grep",
            "-oP",
            R"((?<= - - \[)[0-9]{2}/[A-Za-z]{3}/[0-9]{4})",
            NULL
        };
        execvp("grep", grepAssembly);

        // we should newer hit this
        perror("execution of grep failed");
        exit(1);
    }

    pidTr = fork();
    if (pidTr == -1) {
        perror("fork (tr) failed");
        exit(1);
    }

    if (pidTr == 0) {
        {
            close(parentToGrepPipe[1]); close(parentToGrepPipe[0]);
        }

        {
            close(grepToTrPipe[1]);

            // redirect read end from the pipe to input
            dup2(grepToTrPipe[0], STDIN_FILENO);
            close(grepToTrPipe[0]);
        }

        {
            // closing read end of pipe to parent
            close(trToParentPipe[0]);

            dup2(trToParentPipe[1], STDOUT_FILENO);
            close(trToParentPipe[1]);
        }

        // assembling the tr command
        char *trAssembly[] = {
            "tr",
            "-d",
            "/",
            NULL
        };
        execvp("tr", trAssembly);

        // we should newer hit this either
        perror("execution of tr failed");
        exit(1);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    {
        close(parentToGrepPipe[0]);
        write(parentToGrepPipe[1], inLine, strlen(inLine));

        // we are closing write end here to signal EOF
        close(parentToGrepPipe[1]);
    }

    {
        // closing both ends of pipe in parent process,
        // as we will not need them here
        close(grepToTrPipe[0]); close(grepToTrPipe[1]);
    }

    {
        // also closing unused write end of pipe from tr
        close(trToParentPipe[1]);
    }

    // Wait for both children to finish
    waitpid(pidGrep, NULL, 0);
    waitpid(pidTr, NULL, 0);

    // converting tr write pipe output to file so we could read it with getline
    // it will read only one line, but the cool part is that we don't need any buffers and our memory is safe
    FILE *pipe_stream = fdopen(trToParentPipe[0], "r");

    if (getline(outBuffer, outBufferSize, pipe_stream) == -1) {
        fprintf(stderr, "Failed to extract date: %s\n", *outBuffer);
    }

    fclose(pipe_stream);
    close(trToParentPipe[0]);
}