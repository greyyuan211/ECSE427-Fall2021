// DISCLAIMER: This code is given for illustration purposes only. It can contain bugs!
// You are given the permission to reuse portions of this code in your assignment.
//
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#include <signal.h>
#include <linux/limits.h>
//
// This code is given for illustration purposes. You need not include or follow this
// strictly. Feel free to write better or bug free code. This example code block does not
// worry about deallocating memory. You need to ensure memory is allocated and deallocated
// properly so that your shell works without leaking memory.
//

// we define the job class to use a link list to store all the processes
struct job
{
    pid_t processId;
    char *cmd;
    struct job *next;
};

struct job *firstnode;
pid_t gpid = 0; // a global value to store the pid of the child process in the foreground

// some modifications are made to this given codes. args2 is added as concurerntly input for piping and redirection.
// piping is a boolean flag that tells if we need to use cmdpipe(), redirecting is a boolean flag that tells if we need to use cmdredirect()
int getcmd(char *prompt, char *args[], char *args2[], char **cmd, int *piping, int *redirecting, int *background)
{
    int length = 0, i = 0;
    int m = 0; //added param
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;
    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);
    if (length <= 0)
    {
        free(line); //to avoid memory leak
        exit(-1);
    }
    *cmd = (char *)malloc(sizeof(char) * length);
    strcpy(*cmd, line); //store the input cmd

    // Check if background is specified.
    if ((loc = index(line, '&')) != NULL)
    {
        *background = 1;
        *loc = ' ';
    }
    else
    {
        *background = 0;
    }

    while ((token = strsep(&line, " \t\n")) != NULL)
    {
        for (int j = 0; j < strlen(token); j++)
            if (token[j] <= 32)
                token[j] = '\0';
        if (!(strlen(token) > 0))
            continue;
        if (!(strcmp(token, "|") == 0))
        {
            if (!(strcmp(token, ">") == 0))
            {
                if (!(*piping == 1 | *redirecting == 1))
                {
                    args[i++] = token;
                }
                else
                {
                    args2[m++] = token;
                }
            }
            else
            { // set redirecting flag equal 1 if we detect ">"
                *redirecting = 1;
            }
        }
        else
        { // set piping flag equal 1 if we detect '|'
            *piping = 1;
        }
    }
    args[i] = NULL; // back to origin
    args2[m] = NULL;
    free(token); //avoid memory leak
    return i + m;
}

//function to pipe the commands when "|" is detected by getcmd
void cmdpipe(char *args[], char *args2[])
{
    int pipefds[2];
    int pipeout = pipe(pipefds);//create a pipe
    if (pipeout == -1) //check for pipe openning error
    {
        printf("open the pipe error.\n");
        exit(EXIT_FAILURE);
    }
    pid_t pid = fork();//fist child
    if (pid != 0)
    {
        if (pid <= 0)
        {
            printf("child process fails to create\n");
        }
        else
        {
            pid_t pid2;
            pid2 = fork();//secibd child
            if (pid2 != 0)
            {
                if (pid2 <= 0)
                {
                    exit(EXIT_FAILURE);
                }
                else
                {
                    close(pipefds[1]);
                    close(pipefds[0]);
                    waitpid(pid2, NULL, 0);
                }
            }
            else
            {
                int dup2out = dup2(pipefds[0], STDIN_FILENO); //dup2
                close(pipefds[1]);
                close(pipefds[0]);
                execvp(args2[0], args2);
                exit(EXIT_FAILURE);
            }
        }
    }
    else
    {
        close(STDOUT_FILENO);
        dup2(pipefds[1], STDOUT_FILENO) == -1;
        close(pipefds[0]);
        close(pipefds[1]);
        execvp(args[0], args);
    }
}

// function to redirect a cmd output to a file when getcmd detects ">"
void cmdredirect(char *args[], char *args2[])
{
    pid_t pid;
    pid = fork();
    if (pid != 0)
    {
        if (pid > 0)
        {
            waitpid(pid, NULL, 0);
        }
    }
    else
    {
        int openout = open(args2[0], O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (openout == -1)
        {
            printf("open file error\n");
            exit(EXIT_FAILURE); // when file cannot be opened
        }
        int dup2out = dup2(openout, STDOUT_FILENO);
        if (dup2out == -1)
        {
            exit(EXIT_FAILURE); // when dup2 fails to execute
        }
        close(openout);
        execvp(args[0], args);
    }
}


//handle ctrl+z signal (SIGTSTP)
static void SIGTSTP_handler(int sig)
{
    fprintf(stdout, "we should be ignoring Ctrl+Z");
}

//handle ctrl+c signal (SIGINT)
static void SIGINT_handler(int sig)
{
    if (sig == SIGINT)
    {
        if (gpid <= 0)
        {
            fprintf(stderr, "there is no running fg process.\n");
        }
        else
        {
            fprintf(stderr, "kill %d\n", gpid);
            kill(gpid, SIGKILL);
            gpid = 0; //reset the global flag to 0 back to the origin
        }
    }
}

// *************************************************************************
// functions about jobs

// function to create a new job to add to the job list
struct job *create_job(pid_t ppid, char *cmd)
{
    struct job *aJob = (struct job *)malloc(sizeof(struct job)); //allocate memory for a job
    if (!(aJob != NULL)) // if job is null return immediately
        return aJob;
    // if job is not null, set up job list attributes
    aJob->cmd = cmd;
    aJob->processId = ppid;
    aJob->next = NULL;
    return aJob;
}

// function to kill all bg processes
void kill_all()
{
    struct job *onejob = firstnode;
    // loop until kill all running jobs
    while (onejob != NULL)
    {
        kill(onejob->processId, SIGKILL);//kill the process using the processId
        onejob = onejob->next;//point to the next job
    }
}



// put a job to the singly linked list of jobs
void put_job(pid_t ppid, char *cmd)
{
    struct job *aJob = create_job(ppid, cmd);//create the job to put
    struct job *pointer;
    int position = 1;
    // printf("1position:%d\n",position);
    if (!(firstnode == NULL))
    {
        pointer = firstnode;
        position = position + 1;
        // printf("2position:%d\n",position);
        while (pointer->next != NULL)
        {
            position = position + 1;
            pointer = pointer->next;
            // printf("3position:%d\n",position);
        }
        pointer->next = aJob;
    }
    else
    {
        // printf("4position:%d\n",position);
        firstnode = aJob; // if head is null, just make it point to the job created
    }
    printf("[%d]     %d\n", position, ppid); //print the output as we add the job
}




// function to check if a process if still running
int is_running(pid_t pid)
{
    int running; //boolean to determine if is running
    pid_t waitout = waitpid(pid, &running, WNOHANG);

    if (!(WIFEXITED(running) || WIFSIGNALED(running) &&waitout != 0 ))
        return 0; //if the process is still running give 0
    return -1;    // if the process is not running give -1
}

//check if each job in the list is running or not, and keep only those are running
struct job *update_joblist(struct job *head)
{
    if (head == NULL)
        return head;
    head->next = update_joblist(head->next);//recursively point the next list to the current list
    if (is_running(head->processId) == -1) // the process has stopped
    {
        return head->next;
    }
    else // if the process is running, then return the current node
    {
        return head;
    }
}

// function to output all the information of each job
void print_jobs()
{
    int counter = 1;
    struct job *onejob = firstnode;
    while (onejob != NULL) //loop until all the jobs in the list are printed
    {
        printf("[%d]    %d    %s\n", counter, onejob->processId, onejob->cmd);//print current job
        onejob = onejob->next;//proceed to the next job
        counter++;
    }
}

// helper method to execute cmdfg in the builtincmd function
void fghelper(char *args[])
{
    if (!(args[1] == NULL))
    {
        struct job *onejob = firstnode;
        char *save = args[1];
        int counter = 1;
        while (onejob != NULL)
        {
            if (!(counter == atoi(save)))
            {
                onejob = onejob->next;
                counter = counter + 1;
            }
            else
            {
                gpid = onejob->processId;
                waitpid(onejob->processId, NULL, 0);
                return;
            }
        }
        printf("bg process if not defined in the input");
    }
}

//function to execute built in cmd like cd pwd exit ect.
int cmdbuiltin(char *args[], int cnt)
{
    if (cnt == 0)
    {
        return -1;
    }
    if (!(strcmp(args[0], "cd") == 0))
    {
        if (!(strcmp(args[0], "pwd") == 0))
        {
            if (!(strcmp(args[0], "exit") == 0))
            {
                if (!(strcmp(args[0], "jobs") == 0))
                {
                    if (!(strcmp(args[0], "fg") == 0))
                    { // return 1 when the cmd is not a built in command
                        return 1;
                    }
                    else
                    { // when the cmd is fg
                        firstnode = update_joblist(firstnode);
                        fghelper(args);
                    }
                }
                else
                { // when the cmd is jobs
                    firstnode = update_joblist(firstnode);
                    print_jobs();
                }
            }
            else
            { // when the cmd is exit
                kill_all();
                kill(0, SIGKILL);
            }
        }
        else
        { // when the cmd is pwd
            char cwd[1024];
            if (!(getcwd(cwd, sizeof(cwd)) != NULL))
            {
                return -1;
            }
            else
                printf("%s\n", cwd);
        }
    }
    else
    { // when the cmd is cd
        if (!(args[1] == NULL))
        {
            chdir(args[1]);
        }
        else
        {
            printf("cd directory is not defined\n");
            return -1; // return -1 always if there if an error
        }
    }
    return 0; // return 0 if the command is sucessfully run
}

//function to execute cmd as fg process
void cmdfg(char *args[], char *cmd)
{
    pid_t pid;
    pid = fork();//create child
    if (pid != 0)//if not a child process
    {
        if (pid > 0) //if parent
        {
            gpid = pid;
            waitpid(pid, NULL, 0);
            gpid = 0;
        }
    }
    else
    {
        if (execvp(args[0], args) == -1)//if execvp error
        {
            exit(EXIT_FAILURE);
        }
    }
}

//function for command to run process in the background
void cmdbg(char *args[], char *cmd)
{
    pid_t pid;
    pid = fork();
    if (pid != 0)
    {
        if (pid > 0)
        {
            firstnode = update_joblist(firstnode);
            put_job(pid, cmd);
        }
    }
    else
    {
        if (signal(SIGINT, SIG_IGN) == SIG_ERR)
        {
            printf("fail sigint\n");
            exit(EXIT_FAILURE);
        }
        if (execvp(args[0], args) == -1)
        {
            printf("fail evecup\n");
            exit(EXIT_FAILURE);
        }
    }
}


int main(void)
{
    char *args[20];
    char *args2[20];
    int bg;
    int piping;
    int redirecting;

    char *cmd = NULL;
    firstnode = NULL;

    //ctrl+z
    __sighandler_t tstpout = signal(SIGTSTP, SIGTSTP_handler);
    if (tstpout == SIG_ERR)
    {
        fprintf(stdout, "signal SIGTSTP fails to be handled\n");
        exit(EXIT_FAILURE);
    }

    //ctrl+c
    __sighandler_t intout = signal(SIGINT, SIGINT_handler);
    if (intout == SIG_ERR)
    {
        fprintf(stdout, "signal SIGINT fails to be handled\n");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        bg = 0;
        piping = 0;
        redirecting = 0;
        int cnt = getcmd("\n>> ", args, args2, &cmd, &piping, &redirecting, &bg);
        if (cnt > 0)
        {
            if (cmdbuiltin(args,cnt) != 0 && cmdbuiltin(args,cnt) != -1)
            {
                if (piping != 1)
                {
                    if (!(redirecting == 1))
                    {
                        if (bg != 1) // the command is in the fg process
                        { 
                            cmdfg(args, cmd); 
                        }
                        else // &: the cmmand is in the bg process
                        { 
                            cmdbg(args, cmd);
                        }
                    }
                    else// >: redirection
                    {
                        cmdredirect(args, args2);
                    }
                }
                else // |:piping
                {
                    cmdpipe(args, args2);
                }
            }
            else
            { 
                continue; // continue if the cmd is using cmdbuiltin
            }
        }
    }
}