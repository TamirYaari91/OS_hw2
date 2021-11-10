//
// Created by Tamir Yaari on 04/11/2021.
//

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>


void check_pid(int pid) {
    if (pid < 0) {
        perror("Error: fork has failed");
        exit(1);
    }
}

void signal_handler(int ind) {
    struct sigaction sigstruct;
    memset(&sigstruct, 0, sizeof(sigstruct));
    if (ind == 0) { // handling SIG_CHLD
        sigstruct.sa_sigaction = NULL;
        sigstruct.sa_flags = SA_NOCLDSTOP;
        if (sigaction(SIGCHLD, &sigstruct, 0) == -1) {
            perror("Error: sigaction has failed.");
            exit(1);
        }
    }
    if (ind == 1) { // handling SIG_IGN
        sigstruct.sa_sigaction = (void *) SIG_IGN;
        sigstruct.sa_flags = SA_RESTART;
        if (sigaction(SIGINT, &sigstruct, 0) == -1) {
            perror("Error: sigaction has failed.");
            exit(1);
        }
    }
}

void allow_sigint() { // When needed, allow SIG_INT to terminate process
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, 0) == -1) {
        perror("Error: sigaction has failed.");
    }
}

int background_command(int count, char **arglist) {
    int pid;
    arglist[count - 1] = NULL;
    pid = fork();
    check_pid(pid);
    if (pid == 0) {
        execvp(arglist[0], arglist);
        perror(arglist[0]);
        exit(1);
    }
    return 1; // background command => parent doesn't wait
}

int normal_command(char **arglist) {
    int pid;
    pid = fork();
    check_pid(pid);
    if (pid == 0) {
        allow_sigint();
        execvp(arglist[0], arglist);
        perror(arglist[0]);
        exit(1);
    }
    waitpid(pid, NULL, WUNTRACED); // normal command => parent waits
    return 1;
}

int pipe_command(char **arglist, int pipe_ind) {
    pid_t pid_first;
    pid_t pid_second = -1;
    int pipe_arr[2];
    char **second_command = arglist + pipe_ind + 1;
    arglist[pipe_ind] = NULL;
    if (pipe(pipe_arr) == -1) {
        perror("Error: pipe has failed.");
        exit(1);
    }
    pid_first = fork();
    check_pid(pid_first);
    if (pid_first > 0) {
        close(pipe_arr[1]); // first child only sends => read pipe immediately closed
        pid_second = fork();
        check_pid(pid_second);
    }
    if (pid_first == 0) {
        allow_sigint();
        close(pipe_arr[0]); // second child only receives => write pipe immediately closed
        dup2(pipe_arr[1], 1);
        close(pipe_arr[1]);
        execvp(arglist[0], arglist);
        perror(arglist[0]);
        exit(1);
    }
    if (pid_second == 0) {
        allow_sigint();
        dup2(pipe_arr[0], 0);
        close(pipe_arr[0]);
        execvp(second_command[0], second_command);
        perror(second_command[0]);
        exit(1);
    }
    waitpid(pid_first, NULL, WUNTRACED);
    waitpid(pid_second, NULL, WUNTRACED);
    return 1;
}

int output_redirection_command(int count, char **arglist) {
    int pid, fd, dup2_val;

    fd = open(arglist[count - 1], O_CREAT | O_WRONLY, 0666); // allow the file to be created and written to
    arglist[count - 2] = NULL; // trim the command after getting the file
    if (fd == -1) {
        perror("Error: file opening has failed");
        exit(1);
    }
    pid = fork();
    check_pid(pid);
    if (pid == 0) {
        allow_sigint();
        dup2_val = dup2(fd, STDOUT_FILENO);
        if ((dup2_val == -1) && (errno != ECHILD) && (errno != EINTR)) { // if there is an error in waitpid and it
            // is not a permitted error => exit
            exit(1);
        }
        close(fd);
        execvp(arglist[0], arglist);
        perror(arglist[0]);
        exit(1);
    }
    waitpid(pid, NULL, WUNTRACED);
    close(fd);
    return 1;
}

int process_arglist(int count, char **arglist) {
    int i, pipe_ind = -1;
    if (arglist[count - 1][0] == '&') {
        background_command(count, arglist);
        return 1;
    }
    for (i = 0; i < count; ++i) {
        if (arglist[i][0] == '|') {
            pipe_ind = i;
            break;
        }
    }
    if (pipe_ind >= 0) {
        pipe_command(arglist, pipe_ind);
        return 1;
    }
    if (count > 2 && arglist[count - 2][0] == '>') {
        output_redirection_command(count, arglist);
        return 1;
    }
    normal_command(arglist);
    return 1;
}


// prepare and finalize calls for initialization and destruction of anything required
int prepare(void) {
    signal_handler(0);
    signal_handler(1);
    return 0;
}

int finalize(void) {
    return 0;
};



