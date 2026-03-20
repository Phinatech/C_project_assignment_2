 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
 
#define OUTPUT_FILE   "pipeline_output.txt"
#define DISPLAY_BYTES 1024
 
int main(void)
{
    int    pipefd[2];   /* pipefd[0] = read-end, pipefd[1] = write-end */
    pid_t  pid1, pid2;
    int    status;
 
    /* ------------------------------------------------------------------ */
    /* 1. Create the pipe                                                   */
    /* ------------------------------------------------------------------ */
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
 
    /* ------------------------------------------------------------------ */
    /* 2. Open the output file (parent holds fd; child2 will inherit it)   */
    /* ------------------------------------------------------------------ */
    int outfd = open(OUTPUT_FILE,
                     O_WRONLY | O_CREAT | O_TRUNC,
                     0644);
    if (outfd == -1) {
        perror("open output file");
        exit(EXIT_FAILURE);
    }
 
    /* ------------------------------------------------------------------ */
    /* 3. Fork child 1  →  ps aux                                          */
    /* ------------------------------------------------------------------ */
    pid1 = fork();
    if (pid1 == -1) {
        perror("fork child1");
        exit(EXIT_FAILURE);
    }
 
    if (pid1 == 0) {                        /* ---- child 1 ---- */
        /* Redirect stdout → pipe write-end */
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2 child1");
            exit(EXIT_FAILURE);
        }
        /* Close all pipe fds and the output file – not needed here */
        close(pipefd[0]);
        close(pipefd[1]);
        close(outfd);
 
        char *argv[] = { "ps", "aux", NULL };
        execvp("ps", argv);
        perror("execvp ps");               /* only reached on error */
        exit(EXIT_FAILURE);
    }
 
    /* ------------------------------------------------------------------ */
    /* 4. Fork child 2  →  grep root                                       */
    /* ------------------------------------------------------------------ */
    pid2 = fork();
    if (pid2 == -1) {
        perror("fork child2");
        exit(EXIT_FAILURE);
    }
 
    if (pid2 == 0) {                        /* ---- child 2 ---- */
        /* Redirect stdin  → pipe read-end  */
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2 child2 stdin");
            exit(EXIT_FAILURE);
        }
        /* Redirect stdout → output file    */
        if (dup2(outfd, STDOUT_FILENO) == -1) {
            perror("dup2 child2 stdout");
            exit(EXIT_FAILURE);
        }
        close(pipefd[0]);
        close(pipefd[1]);
        close(outfd);
 
        char *argv[] = { "grep", "root", NULL };
        execvp("grep", argv);
        perror("execvp grep");
        exit(EXIT_FAILURE);
    }
 
    /* ------------------------------------------------------------------ */
    /* 5. Parent: close its copies of pipe and output file                 */
    /*    (critical – otherwise grep never sees EOF on its stdin)          */
    /* ------------------------------------------------------------------ */
    close(pipefd[0]);
    close(pipefd[1]);
    close(outfd);
 
    /* ------------------------------------------------------------------ */
    /* 6. Wait for both children                                           */
    /* ------------------------------------------------------------------ */
    printf("[parent] Waiting for child1 (ps aux),  PID %d ...\n", pid1);
    waitpid(pid1, &status, 0);
    printf("[parent] child1 exited with status %d\n", WEXITSTATUS(status));
 
    printf("[parent] Waiting for child2 (grep root), PID %d ...\n", pid2);
    waitpid(pid2, &status, 0);
    printf("[parent] child2 exited with status %d\n", WEXITSTATUS(status));
 
    /* ------------------------------------------------------------------ */
    /* 7. Read and display first DISPLAY_BYTES of the captured output      */
    /* ------------------------------------------------------------------ */
    int readfd = open(OUTPUT_FILE, O_RDONLY);
    if (readfd == -1) {
        perror("open output file for reading");
        exit(EXIT_FAILURE);
    }
 
    char    buf[DISPLAY_BYTES + 1];
    ssize_t n = read(readfd, buf, DISPLAY_BYTES);
    close(readfd);
 
    if (n < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }
 
    buf[n] = '\0';
    printf("\n[parent] === First %zd bytes from %s ===\n\n%s\n",
           n, OUTPUT_FILE, buf);
    printf("[parent] Full output saved to: %s\n", OUTPUT_FILE);
 
    return 0;
}
 