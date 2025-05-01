/*
    COMP3511 Spring 2024
    PA1: Simplified Linux Shell (MyShell)

    Your name:          Wong Hei Hung
    Your ITSC email:    hhwongas@connect.ust.hk

    Declaration:

    I declare that I am not involved in plagiarism
    I understand that both parties (i.e., students providing the codes and students copying the codes) will receive 0 marks.

*/

/*
    Header files for MyShell
    Necessary header files are included.
    Do not include extra header files
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h> // For constants that are required in open/read/write/close syscalls
#include <sys/wait.h> // For wait() - suppress warning messages
#include <fcntl.h>    // For open/read/write/close syscalls

#define MYSHELL_WELCOME_MESSAGE "COMP3511 PA1 Myshell (Spring 2024)"

// Define template strings so that they can be easily used in printf
//
// Usage: assume pid is the process ID
//
//  printf(TEMPLATE_MYSHELL_START, pid);
//
#define TEMPLATE_MYSHELL_START "Myshell (pid=%d) starts\n"
#define TEMPLATE_MYSHELL_END "Myshell (pid=%d) ends\n"
#define TEMPLATE_MYSHELL_CD_ERROR "Myshell cd command error\n"

// Assume that each command line has at most 256 characters (including NULL)
#define MAX_CMDLINE_LENGTH 256

// Assume that we have at most 8 arguments
#define MAX_ARGUMENTS 8

// Assume that we only need to support 2 types of space characters:
// " " (space) and "\t" (tab)
#define SPACE_CHARS " \t"

// The pipe character
#define PIPE_CHAR "|"

// Assume that we only have at most 8 pipe segements,
// and each segment has at most 256 characters
#define MAX_PIPE_SEGMENTS 8

// Assume that we have at most 8 arguments for each segment
// We also need to add an extra NULL item to be used in execvp
// Thus: 8 + 1 = 9
//
// Example:
//   echo a1 a2 a3 a4 a5 a6 a7
//
// execvp system call needs to store an extra NULL to represent the end of the parameter list
//
//   char *arguments[MAX_ARGUMENTS_PER_SEGMENT];
//
//   strings stored in the array: echo a1 a2 a3 a4 a5 a6 a7 NULL
//
#define MAX_ARGUMENTS_PER_SEGMENT 9

// Define the standard file descriptor IDs here
#define STDIN_FILENO 0  // Standard input
#define STDOUT_FILENO 1 // Standard output

// This function will be invoked by main()
void show_prompt(char *prompt, char *path)
{
    printf("%s %s> ", prompt, path);
}

// This function will be invoked by main()
// This function is given
int get_cmd_line(char *cmdline)
{
    int i;
    int n;
    if (!fgets(cmdline, MAX_CMDLINE_LENGTH, stdin))
        return -1;
    // Ignore the newline character
    n = strlen(cmdline);
    cmdline[--n] = '\0';
    i = 0;
    while (i < n && cmdline[i] == ' ')
    {
        ++i;
    }
    if (i == n)
    {
        // Empty command
        return -1;
    }
    return 0;
}

// parse_arguments function is given
// This function helps you parse the command line
//
// Suppose the following variables are defined:
//
// char *pipe_segments[MAX_PIPE_SEGMENTS]; // character array buffer to store the pipe segements
// int num_pipe_segments; // an output integer to store the number of pipe segment parsed by this function
// char cmdline[MAX_CMDLINE_LENGTH]; // The input command line
//
// Sample usage:
//
//  parse_arguments(pipe_segments, cmdline, &num_pipe_segments, "|");
//
void parse_arguments(char **argv, char *line, int *numTokens, char *delimiter)
{
    int argc = 0;
    char *token = strtok(line, delimiter);
    while (token != NULL)
    {
        argv[argc++] = token;
        token = strtok(NULL, delimiter);
    }
    *numTokens = argc;
}

void process_cmd(char *cmdline)
{
    // Uncomment this line to show the content of the cmdline
    printf("cmdline is: %s\n", cmdline);

    char *pipe_segments[MAX_PIPE_SEGMENTS];
    int num_pipe_segments = 0;
    char pipeline[MAX_CMDLINE_LENGTH];
    strcpy(pipeline,cmdline);
    parse_arguments(pipe_segments, pipeline, &num_pipe_segments, PIPE_CHAR);

    int pfds[num_pipe_segments][2];
    int pid; 
    for (int i = 0; i < num_pipe_segments; i++)
        pipe(pfds[i]);
    
    for (int i = 0; i < num_pipe_segments; i++)
    {        
        pid = fork(); 
        
        if ( pid == 0 ) 
        {                   //children
            if(num_pipe_segments > 1)
            {
                if (i == 0) 
                {
                    dup2(pfds[i][1], 1);    // write curr pipe
                    close(pfds[i][0]);
                    for(int j = 1; j < num_pipe_segments; j++)
                    {
                        close(pfds[j][0]);
                        close(pfds[j][1]);
                    }
                }
                else if(i == num_pipe_segments - 1)
                {
                    dup2(pfds[i-1][0], 0);  //read prev pipe
                    close(pfds[i-1][1]);
                    for(int j = 0; j < num_pipe_segments - 1; j++)
                    {
                        close(pfds[j][0]);
                        close(pfds[j][1]);
                    }

                } 
                else
                {
                    dup2(pfds[i-1][0], 0);  //read prev pipe
                    close(pfds[i-1][1]);
                    dup2(pfds[i][1], 1);    // write curr pipe
                    for(int j = 0; j < num_pipe_segments; j++)
                    {
                        if(j != i && j != i-1)
                        {
                            close(pfds[j][0]);
                            close(pfds[j][1]);
                        }
                    }
                }
            }
        
            char *cmd_segments[MAX_ARGUMENTS];
            int num_cmd_segments = 0;
            char line[MAX_CMDLINE_LENGTH];
            strcpy(line,pipe_segments[i]);
            parse_arguments(cmd_segments, line, &num_cmd_segments, SPACE_CHARS);

            char *action = cmd_segments[0];
            int first_symbol = num_cmd_segments;

            if(num_pipe_segments == 1)
            {
                for(int i = 0; i < num_cmd_segments; i++)
                {
                    if(strcmp(cmd_segments[i], "<") == 0) 
                    {//i
                        if(first_symbol == num_cmd_segments) 
                            first_symbol = i;
                        int fd = open(cmd_segments[i+1], O_CREAT | O_RDWR ,S_IRUSR | S_IWUSR ); 
                        dup2(fd, 0);
                    }
                    else if(strcmp(cmd_segments[i], ">") == 0)
                    {//o
                        if(first_symbol == num_cmd_segments) 
                            first_symbol = i;
                        int fd = open(cmd_segments[i+1], O_CREAT | O_RDWR ,S_IRUSR | S_IWUSR ); 
                        dup2(fd, 1);
                    }
                }
            }
            char* args[num_cmd_segments + 1];
            for(int i = 0; i < first_symbol; i++)
                args[i] = cmd_segments[i];
            args[first_symbol] = NULL;
            execvp(action, args);
        }
    }
    
    for (int i = 0; i < num_pipe_segments; i++)
    {  
        close(pfds[i][0]);
        close(pfds[i][1]);
    }
    for (int i = 0; i < num_pipe_segments; i++)
    {  
        wait(NULL);
    }
    
    exit(0); // ensure the process cmd is finished
}

/* The main function implementation */
int main()
{
    // TODO: replace the shell prompt with your ITSC account name
    // For example, if you ITSC account is cspeter@connect.ust.hk
    // You should replace ITSC with cspeter
    char *prompt = "hhwongas";
    char cmdline[MAX_CMDLINE_LENGTH];
    char path[256]; // assume path has at most 256 characters

    printf("%s\n\n", MYSHELL_WELCOME_MESSAGE);
    printf(TEMPLATE_MYSHELL_START, getpid());

    // The main event loop
    while (1)
    {
        getcwd(path, 256);
        show_prompt(prompt, path);

        if (get_cmd_line(cmdline) == -1)
            continue; // empty line handling, continue and do not run process_cmd

        // TODO: Before running process_cmd
        //
        // (1) Handle the exit command
        // (2) Handle the cd command
        //
        // Note: These 2 commands should not be handled by process_cmd
        // Hint: You may call break or continue when handling (1) and (2) so that process_cmd below won't be executed

        char *path_segments[MAX_ARGUMENTS];
        int num_path_segments;
        char line[MAX_CMDLINE_LENGTH];
        strcpy(line,cmdline);
        parse_arguments(path_segments, line, &num_path_segments, SPACE_CHARS);
        char *action = path_segments[0];

        if (strcmp(action, "exit") == 0)
        {
            printf(TEMPLATE_MYSHELL_END, getpid());
            break;
        }

        if (strcmp(action, "cd") == 0)
        {
            
            if (chdir(path_segments[1]) == -1) {
                printf(TEMPLATE_MYSHELL_CD_ERROR);
            }
            continue;
        }
        
        

        pid_t pid = fork();
        if (pid == 0)
        {
            // the child process handles the command
            process_cmd(cmdline);
        }
        else
        {
            // the parent process simply wait for the child and do nothing
            wait(0);
        }
    }

    return 0;
}
