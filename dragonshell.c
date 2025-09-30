#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include "dragonshell.h"

/**
 * @brief Tokenize a C string 
 * 
 * @param str - The C string to tokenize 
 * @param delim - The C string containing delimiter character(s) 
 * @param argv - A char* array that will contain the tokenized strings
 * Make sure that you allocate enough space for the array.
 */
void tokenize(char* str, const char* delim, char ** argv) {
  char* token;
  token = strtok(str, delim);
  for(size_t i = 0; token != NULL; ++i){
    argv[i] = token;
    token = strtok(NULL, delim);
  }
}
pid_t foregroundpid = 0; //pid of the foreground process
typedef enum { 
  R,
  T
}process_status;

typedef struct process {
  pid_t pid;
  process_status status;
  char command[LINE_LENGTH];
  struct process *nextprocess;
}process;

process *process_train = NULL;

void add_process(pid_t pid, process_status status, char* command) {
  process *newprocess = malloc(sizeof(process));
  newprocess->pid = pid;
  newprocess->status = status;
  strncpy(newprocess->command, command, sizeof(newprocess->command) - 1);
  newprocess->nextprocess = process_train;
  process_train = newprocess;
}
void remove_process(pid_t pid) {
  process *current = process_train;
  process *previous = NULL;
  while (current != NULL) {
    if (current->pid == pid) {
      if (previous == NULL) {
        process_train = current->nextprocess;
      } else {
        previous->nextprocess = current->nextprocess;
      }
      free(current);
      return;
    }
    previous = current;
    current = current->nextprocess;
  }
}
void show_process(){
  process *currentprocess = process_train;
  while (currentprocess != NULL) {
    printf("%d ", currentprocess->pid);
    if (currentprocess->status == R) { //running
      printf("R ");
    } else if (currentprocess->status == T) { //suspended
      printf("T ");
    }
    printf("%s\n", currentprocess->command);
    currentprocess = currentprocess->nextprocess;
  }
}
void clear_finished_process() { //clears all finished processes from the process list in background
  process *currentprocess = process_train;
  while (currentprocess != NULL) {
    while (waitpid(currentprocess->pid, NULL, WNOHANG) > 0) {
      remove_process(currentprocess->pid);
    }
    currentprocess = currentprocess->nextprocess;
  }
}
void clear_all_process() { //clears all processes from the process list
  process *currentprocess = process_train;
  while (currentprocess != NULL) {    
    remove_process(currentprocess->pid);
    process *temp = currentprocess;
    currentprocess = currentprocess->nextprocess;
    free(temp);
  }
  process_train = NULL;
}
void CtrlC(int sig) {
  
  if (foregroundpid > 0) {
    kill(foregroundpid, SIGINT);
    printf("\n");
    fflush(stdout);
    foregroundpid = 0;
  } else {
    printf("\ndragonshell>");
    fflush(stdout);
  }

  return;
}
void CtrlZ(int sig) {
  if (foregroundpid > 0) {
    kill(foregroundpid, SIGTSTP);
    printf("\n");
    fflush(stdout);
    foregroundpid = 0;
  } else {
    printf("\ndragonshell>");
    fflush(stdout);
  }
  return; 
}
int main(int argc, char **argv) {
  printf("Welcome to Dragon Shell!\n");
  printf("\n");

  extern char **environ; // environment variables
  signal(SIGINT, CtrlC);
  signal(SIGTSTP, CtrlZ);
  while(1) {
    char input[LINE_LENGTH];
    char* args[MAX_ARGS];
    char command[LINE_LENGTH];
    char filename[LINE_LENGTH];
    filename[0] = '\0'; 
    command[0] = '\0'; 
    int background = 0;
    int pipeflag=0;
    signal(SIGINT, CtrlC);
    signal(SIGTSTP, CtrlZ);
    printf("dragonshell>");
    fflush(stdout); // force the output to be printed immediately
    if (fgets(input, sizeof(input), stdin) == NULL) {
      printf("\n");
      break;
      }
    
    for (int i = 0; i < MAX_ARGS; i++) {
        args[i] = NULL;
    }
    tokenize(input, " \n", args);
    for (int i = 0; i<MAX_ARGS; i++) {
      if (args[i] == NULL) {
        break;
      }
      strcat(command, args[i]);
      strcat(command, " ");
    }
    #pragma region foreground/background
    int n = 0;
    while (args[n] != NULL) { 
      if (strcmp(args[n], "&") ==0) {
        background = 1;
        args[n] = NULL;
      } else {
      background = 0;
      }
      n++;
    }
    n=0;
    #pragma endregion
    #pragma region redirection
    int redirection = 0;
    while (args[n] != NULL) {
      if (strcmp(args[n], ">") ==0) {
        redirection = 1;
        args[n] = NULL;
        if (args[n+1] != NULL) {
          strncpy(filename, args[n+1], sizeof(filename) - 1);
          filename[sizeof(filename) - 1] = '\0';
          args[n+1] = NULL;
        } else {
          fprintf(stderr, "Expected argument to \" > \" \n");
        }
      } else if (strcmp(args[n], "<") ==0) {
        redirection = 2;
        args[n] = NULL;
        if (args[n+1] != NULL) {
          strncpy(filename, args[n+1], sizeof(filename) - 1);
          filename[sizeof(filename) - 1] = '\0';
          args[n+1] = NULL;
        } else {
          fprintf(stderr, "Expected argument to \" < \" \n");
        }
      }
      n++;
    }
    #pragma endregion
    #pragma region pipe
    n = 0;
    int pipeindex = 0;
    char* firstcommand[MAX_ARGS];
    char* secondcommand[MAX_ARGS];
    while (args[n] != NULL) {
      if (strcmp(args[n], "|")==0) {
        pipeflag = 1;
        pipeindex = n;
        args[n] = NULL;
        break;
      } else
      n++;
    }
    for (int i = 0; i < pipeindex; i++) {
      firstcommand[i] = args[i];
    }
    firstcommand[pipeindex] = NULL;
    int j = 0;
    for (int i=pipeindex+1; args[i] != NULL; i++) {
      secondcommand[j] = args[i];
      j++;
    }
    secondcommand[j] = NULL;
    #pragma endregion

    if (strcmp(args[0], "pwd") == 0) {
      char cwd[1024];
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
      } else {
        perror("getcwd() error");
      }

    } else if (strcmp(args[0], "exit") == 0) {
      clear_finished_process();
      clear_all_process();
      break;
      
    } else if (strcmp(args[0], "cd") == 0) {
      if (args[1] == NULL) {
        fprintf(stderr, "cd: expected argument\n");
      } else {
        if (chdir(args[1]) != 0) {
          printf("Expected argument to \"cd\" \n");
          perror("dragonshell");
        }
      }
    } else if (strcmp(args[0], "jobs") == 0) {
      clear_finished_process();
      show_process();
    } 
    else {
      if (access(args[0], X_OK)==0) {
        if (pipeflag == 0) {
          pid_t pid = fork();
                
          if (pid > 0) { // parent process             
            if (background == 1) { //background process
              add_process(pid, R, command);
              printf("PID %d is sent to the background\n", pid);
              continue;
            } else if (background == 0) { //foreground process              
              add_process(pid, R, command);
              foregroundpid = pid;
              int status;
              waitpid(pid, &status, WUNTRACED);
              
                if(WIFSTOPPED(status)) {                  
                  process *currentprocess = process_train;
                  while (currentprocess != NULL) {
                    if (currentprocess->pid == pid) {
                      currentprocess->status = T; //change status to suspended
                      break;
                    }
                  currentprocess = currentprocess->nextprocess;
                  }
                  continue;
                } else {
                remove_process(pid);
                continue;
              }
            
          }
          } else if ((pid == 0)) { // child process
            if (background == 1) {
              freopen("/dev/null", "w", stdout);
            }
           if (redirection == 1) {
            int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
              perror("Open for output redirection");
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
          } else if (redirection == 2) {
            int fd = open(filename, O_RDONLY);
            if (fd < 0) {
              perror("Open for input redirection");
            } 
          dup2(fd, STDIN_FILENO);
          close(fd);
          }
          execve(args[0], args, environ);
          perror("dragonshell");
          exit(EXIT_FAILURE);
          } 
        } else if ((pipeflag == 1)) {
          int pipefd[2];
          pipe(pipefd);
          pid_t pid2 = fork();
          if (pid2 == 0) {
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);
            execve(firstcommand[0], firstcommand, environ);
            perror("dragonshell");
            exit(EXIT_FAILURE);
          } 
          pid_t pid3 = fork();
          if (pid3 == 0) {
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[1]);
            close(pipefd[0]);
            execve(secondcommand[0], secondcommand, environ);
            perror("dragonshell");
            exit(EXIT_FAILURE);
          } 
          close(pipefd[0]);
          close(pipefd[1]);  
          waitpid(pid2, NULL, 0);
          waitpid(pid3, NULL, 0);      
        }
      } else if (access(args[0], X_OK)!=0) {
        fprintf(stderr, "%s: command not found\n", "dragonshell");     
      }
    }   
  // print the string prompt without a newline, before beginning to read
  // tokenize the input, run the command(s), and print the result
  // do this in a loop
  }
  return 0;
}