#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMDLINE_MAX 512

#define SUCCESS 0
#define FAILURE 1

#define MAX_PIPES 3
#define MAX_COMMANDS 4

struct Job{
    /* Used to store information about the job that is returning. Has the
    locations of all the special characters */

    int pipes[MAX_PIPES];
    int append;
    int redirect;
    int background;
    int numCommands;
};

struct BackgroundProcess{
    /* Used to store all background processes that are currently running
    and particular information about each process */

    char *cmd;
    pid_t pids[MAX_COMMANDS];
    int status[MAX_COMMANDS];
    int numCommands;
    struct BackgroundProcess *next;
};

void printBuiltInMessage(char *cmd, int retval){
    // Used to print completion message for built-in commands

    fprintf(stderr, "+ completed \'%s\' [%d]\n", cmd,
            retval);
}

void printMessage(char *cmd, int *status, int numCommands){
    // Used to print completion message for all non-built-in commands

    fprintf(stderr, "+ completed \'%s\' ", cmd);
    for (int i = 0; i < numCommands; ++i)
    {
        fprintf(stderr, "[%d]", WEXITSTATUS(status[i]));
    }
    fprintf(stderr, "\n");
}

void pwdCommand(){
    // Executes the built-in pwd command

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    fprintf(stdout, "%s\n", cwd);
}

int cdCommand(char *newDirectory){
    /*  Executes to cd Command. If the provided directory cannot
    be changed to this function will return 1. If successful it will return 0 */

    if (chdir(newDirectory)){
        fprintf(stderr, "Error: cannot cd into directory\n");
        return 1;
    }
    return 0;
}

void freeArgs(char **args){
    // Free the array of strings containing the arguments for a process

    for (int i = 0; i < 25; ++i){
        if (args[i] == NULL)
            continue;
        free(args[i]);
    }
    free(args);
}

int checkTooManyArgs(char *cmd){
    // Checks to see if more than 16 arguments were provided

    char *whitespace_cmd = malloc((strlen(cmd) + 1) * sizeof(char));
    strcpy(whitespace_cmd, cmd);
    char *token = strtok(whitespace_cmd, " ");

    int i = 0;
    while (token != NULL){
        i++;
        token = strtok(NULL, " ");
    }
    free(whitespace_cmd);

    if (i > 16){
        fprintf(stderr, "Error: too many process arguments\n");
        return 1;
    }
    return 0;
}

void addWordToArgs(int *arg_length, int *args_index, char *itr, char *start, char **args){
    // Adds a word that provided in cmd to the args array

    *arg_length += 1;
    *itr = '\0';
    args[*args_index] = malloc(*arg_length * sizeof(char));
    strcpy(args[*args_index], start);
    *arg_length = 0;
    *args_index += 1;
}

void addSpecialChars(char **args, int *args_index, char *specialChar, int size){
    // Adds a special character ( <, <<, |, &) to the args array

    args[*args_index] = malloc(size * sizeof(char));
    strcpy(args[*args_index], specialChar);
    *args_index += 1;
}

void parseCmd(char **args, char *parse_str){
    /* Parses the cmdline that was initially provided character by character
    and adds it to the args array. The args array is an array of strings that is split
    by whitespace or the appearance of a special character*/

    char *start = parse_str;
    char *itr = parse_str;
    int args_index = 0;
    int arg_length = 0;
    while (*itr != '\0'){

        if (*itr == '>'){
            if (arg_length > 0){
                addWordToArgs(&arg_length, &args_index, itr, start, args);
            }
            if (*(itr + 1) != '\0' && *(itr + 1) == '>'){
                addSpecialChars(args, &args_index, ">>", 3);
                itr += 2;
            }
            else{
                addSpecialChars(args, &args_index, ">", 2);
                itr++;
            }
            start = itr;
        }

        else if (*itr == '|'){
            if (arg_length > 0){
                addWordToArgs(&arg_length, &args_index, itr, start, args);
            }
            addSpecialChars(args, &args_index, "|", 2);
            start = ++itr;
        }

        else if (*itr == '&'){
            if (arg_length > 0){
                addWordToArgs(&arg_length, &args_index, itr, start, args);
            }
            addSpecialChars(args, &args_index, "&", 2);
            start = ++itr;
        }

        else if (*itr == ' '){
            if (arg_length > 0){
                addWordToArgs(&arg_length, &args_index, itr, start, args);
            }
            start = ++itr;
        }

        else{
            itr++;
            arg_length++;
        }
    }

    if (arg_length > 0){
        arg_length++;
        args[args_index] = malloc(arg_length * sizeof(char));
        strcpy(args[args_index], start);
        args_index++;
    }
}

char **getArgs(char *cmd){
    /* Returns NULL if there are no arguments provided or there are too many arguments.
    Else the args array will be returned containing the strings split by word or
    special character*/

    if (checkTooManyArgs(cmd)){
        return NULL;
    }

    char **args = malloc(25 * sizeof(char *));
    for (int i = 0; i < 25; i++){
        args[i] = NULL;
    }

    char *parse_str = malloc((strlen(cmd) + 1) * sizeof(char));
    strcpy(parse_str, cmd);

    parseCmd(args, parse_str);
    free(parse_str);

    if (args[0] == NULL){
        freeArgs(args);
        return NULL;
    }
    return args;
}

void InitializeJob(struct Job *job){

    // Intializes all the of the data in the job struct to -1

    for (int i = 0; i < MAX_PIPES; ++i){
        job->pipes[i] = -1;
    }
    job->append = -1;
    job->redirect = -1;
    job->background = -1;
    job->numCommands = 1;
}

void findAllSpecialCharLocations(char **args, struct Job *job){

    /* Finds the location of all special characters in the args array
       and sets the relevant member in the job struct to it*/

    int i = 0;
    int pipeIndex = 0;

    while (args[i] != NULL){

        if (!strcmp(args[i], ">>")){
            job->append = i;
        }

        else if (!strcmp(args[i], ">")){
            job->redirect = i;
        }

        else if (!strcmp(args[i], "&")){
            job->background = i;
        }

        else if (!strcmp(args[i], "|")){
            job->pipes[pipeIndex++] = i;
        }

        i++;
    }
}

int checkAtIndexZero(struct Job *job){
    // Check if there is a special character at the first character in the job

    if (job->pipes[0] == 0 || job->append == 0 || job->redirect == 0 || job->background == 0){
        fprintf(stderr, "Error: missing command\n");
        return FAILURE;
    }
    return SUCCESS;
}

int checkBetweenPipes(struct Job *job){
    // Check for potential parsing errors if pipes are present

    int pipeLocation = 0;
    while (job->pipes[pipeLocation] != -1 && pipeLocation < 3){
        int index = job->pipes[pipeLocation];

        if (index - 1 == job->append || index - 1 == job->redirect){
            fprintf(stderr, "Error: no output file\n");
            return FAILURE;
        }

        if (job->background != -1 && job->background < index){
            fprintf(stderr, "Error: mislocated background sign\n");
            return FAILURE;
        }

        if ((job->append != -1 && job->append < index) || (job->redirect != -1 && job->redirect < index)){
            fprintf(stderr, "Error: mislocated output redirection\n");
            return FAILURE;
        }

        if (index + 1 == job->append || index + 1 == job->redirect || index + 1 == job->background){
            fprintf(stderr, "Error: missing command\n");
            return FAILURE;
        }
        pipeLocation++;
    }

    return SUCCESS;
}

int checkLastIndex(char *args[], struct Job *job){

    // Check for parsing errors at the last index of the args array

    int lastCommand = 0;
    while (args[lastCommand] != NULL){
        lastCommand++;
    }
    lastCommand--;

    if (job->background != -1 && job->background != lastCommand){
        fprintf(stderr, "Error: mislocated background sign\n");
        return FAILURE;
    }

    if (job->pipes[(job->numCommands - 1)] == lastCommand){
        fprintf(stderr, "Error: missing command\n");
        return FAILURE;
    }

    if (job->redirect == lastCommand || job->append == lastCommand){
        fprintf(stderr, "Error: no output file\n");
        return FAILURE;
    }

    return SUCCESS;
}

int openableFile(char *args[], struct Job *job){
    // If output redirect/append is true, then check if the file is openable

    if (job->redirect != -1){
        int fd = open(args[job->redirect + 1], O_WRONLY | O_CREAT, 0666);
        if (fd == -1){
            fprintf(stderr, "Error: cannot open output file\n");
            return FAILURE;
        }
        close(fd);
    }

    else if (job->append != -1){
        int fd = open(args[job->append + 1], O_WRONLY | O_CREAT, 0666);
        if (fd == -1){
            fprintf(stderr, "Error: cannot open output file\n");
            return SUCCESS;
        }
        close(fd);
    }

    return 0;
}

int checkParsingError(char *args[], struct Job *job){
    // Returns 1 if any of the helper function detects a parsing error

    if (checkAtIndexZero(job) || checkBetweenPipes(job) || checkLastIndex(args, job) || openableFile(args, job))
        return FAILURE;

    return SUCCESS;
}

void freeAllSpecialChars(char *args[], struct Job *job){
    /* Sets all the special characters in the args array to NULL
    and frees the dynamically allocated space for that character*/

    for (int i = 0; i < 3; i++){
        if (job->pipes[i] == -1)
            break;
        free(args[job->pipes[i]]);
        args[job->pipes[i]] = NULL;
        job->numCommands++;
    }

    if (job->redirect != -1) {
        free(args[job->redirect]);
        args[job->redirect] = NULL;
    }

    if (job->append != -1){
        free(args[job->append]);
        args[job->append] = NULL;
    }

    if (job->background != -1){
        free(args[job->background]);
        args[job->background] = NULL;
    }
}

struct BackgroundProcess *newBackground(char *cmd, int *status, pid_t *pids, int numCommands){
    // Create a new Background Process object

    struct BackgroundProcess *nextBackground = malloc(sizeof(struct BackgroundProcess));
    nextBackground->cmd = malloc((strlen(cmd) + 1) * sizeof(char));
    strcpy(nextBackground->cmd, cmd);
    memcpy(nextBackground->pids, pids, MAX_COMMANDS * sizeof(pid_t));
    memcpy(nextBackground->status, status, MAX_COMMANDS * sizeof(int));
    nextBackground->numCommands = numCommands;
    nextBackground->next = NULL;
    return nextBackground;
}

void addNewBackground(struct BackgroundProcess **head, struct BackgroundProcess *nextCmd){
    /* Adds new a background process to the linked list of all running
      background processes */

    if (*head == NULL){
        *head = nextCmd;
        return;
    }

    struct BackgroundProcess *itr = *head;
    while (itr->next){
        itr = itr->next;
    }
    itr->next = nextCmd;
}

void freeBackground(struct BackgroundProcess *remove){
    // Frees dynamically allocated space for a background process

    free(remove->cmd);
    free(remove);
}

void removeBackground(struct BackgroundProcess **head, struct BackgroundProcess *removeCmd){
    /* Traverse thorugh the Background Process linked list. If the current process equals the one
    to be removed, then remove it from the linked list*/

    if ((*head) == removeCmd){
        struct BackgroundProcess *free_process = *head;
        *head = (*head)->next;
        freeBackground(free_process);
        return;
    }

    struct BackgroundProcess *itr = (*head)->next;
    struct BackgroundProcess *prev = (*head);

    while (itr){
        if (itr == removeCmd){
            struct BackgroundProcess *free_process = itr;
            prev->next = itr->next;
            freeBackground(free_process);
            return;
        }
        prev = itr;
        itr = itr->next;
    }
}

int checkCompletion(struct BackgroundProcess *backgroundCmd){
    // Check to see if all processes in a job have finsished for background commands only

    for (int i = 0; i < backgroundCmd->numCommands; ++i){
        if (backgroundCmd->pids[i] != -1 && waitpid(backgroundCmd->pids[i], &(backgroundCmd->status[i]), WNOHANG)){
            backgroundCmd->pids[i] = -1;
        }
    }

    for (int i = 0; i < backgroundCmd->numCommands; ++i){
        if (backgroundCmd->pids[i] != -1){
            return 0;
        }
    }
    printMessage(backgroundCmd->cmd, backgroundCmd->status, backgroundCmd->numCommands);
    return 1;
}

void checkAllBackground(struct BackgroundProcess **head){
    /*  Check if all background processes have finished. If a process
    has finished, remove it from the linked list*/

    struct BackgroundProcess *itr = *head;
    while (itr){
        if (checkCompletion(itr)){
            struct BackgroundProcess *remove = itr;
            itr = itr->next;
            removeBackground(head, remove);
        }
        else{
            itr = itr->next;
        }
    }
}

void checkOutputRedirect(char **args, struct Job *job){
    /* Checks to see if the redirect/append operator is provided
    in the command. If it is there it will redirect the ouput to the
    specified file*/

    int fd;
    if (job->redirect != -1){
        fd = open(args[job->redirect + 1], O_TRUNC | O_WRONLY | O_CREAT, 0666);
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    else if (job->append != -1){
        fd = open(args[job->append + 1], O_WRONLY | O_CREAT | O_APPEND, 0666);
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

void closePipes(int *pipe1, int *pipe2, int *pipe3){
    // Closes all open pipes for a process

    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);
    close(pipe3[0]);
    close(pipe3[1]);
}

void getStatus(pid_t *pids, int *status, int numCommands){
    /* Gets the status for all commands that are not built-in
     or to be run in the background*/

    for (int i = 0; i < numCommands; i++){
        waitpid(pids[i], &status[i], 0);
    }
}

int *executeCommands(char *cmd, char *args[], struct Job *job, struct BackgroundProcess **head){
    /*It will fork and execute the  based on the number of commands that are in a job. If it is background
    process, the function will not wait for the child processes to finish, and it will add a new process
    to the background process linked list. The opposite holds true if it is not a background process.
    The function will also correctly redirect the output to a file for the last command in a job if required.*/

    int pipe1[2], pipe2[2], pipe3[2];
    pipe(pipe1);
    pipe(pipe2);
    pipe(pipe3);

    pid_t pids[MAX_COMMANDS];

    pids[0] = fork();
    if (pids[0] == 0){
        if (job->numCommands == 1){
            checkOutputRedirect(args, job);
        }
        else{
            dup2(pipe1[1], STDOUT_FILENO);
        }

        closePipes(pipe1, pipe2, pipe3);
        execvp(args[0], args);
        fprintf(stderr, "Error: command not found\n");
        exit(FAILURE);
    }

    if (job->numCommands > 1){
        pids[1] = fork();
        if (pids[1] == 0){
            dup2(pipe1[0], STDIN_FILENO);

            if (job->numCommands == 2){
                checkOutputRedirect(args, job);
            }
            else{
                dup2(pipe2[1], STDOUT_FILENO);
            }

            closePipes(pipe1, pipe2, pipe3);
            execvp(args[job->pipes[0] + 1], &args[job->pipes[0] + 1]);
            fprintf(stderr, "Error: command not found\n");
            exit(FAILURE);
        }
    }

    if (job->numCommands > 2){
        pids[2] = fork();
        if (pids[2] == 0){
            dup2(pipe2[0], STDIN_FILENO);

            if (job->numCommands == 3){
                checkOutputRedirect(args, job);
            }
            else{
                dup2(pipe3[1], STDOUT_FILENO);
            }

            closePipes(pipe1, pipe2, pipe3);
            execvp(args[job->pipes[1] + 1], &args[job->pipes[1] + 1]);
            fprintf(stderr, "Error: command not found\n");
            exit(FAILURE);
        }
    }

    if (job->numCommands > 3){
        pids[3] = fork();
        if (pids[3] == 0){
            dup2(pipe3[0], STDIN_FILENO);
            checkOutputRedirect(args, job);

            closePipes(pipe1, pipe2, pipe3);
            execvp(args[job->pipes[2] + 1], &args[job->pipes[2] + 1]);
            fprintf(stderr, "Error: command not found\n");
            exit(FAILURE);
        }
    }

    closePipes(pipe1, pipe2, pipe3);
    int *status = malloc(MAX_COMMANDS * sizeof(int));

    if (job->background == -1){
        getStatus(pids, status, job->numCommands);
        return status;
    }

    else{
        struct BackgroundProcess *nextBackground = newBackground(cmd, status, pids, job->numCommands);
        addNewBackground(head, nextBackground);
        free(status);
        return NULL;
    }
}

int main(void){
    char cmd[CMDLINE_MAX];
    struct BackgroundProcess *head = NULL;

    while (1){
        char *nl;
        /* Print prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

        /* Get command line */
        fgets(cmd, CMDLINE_MAX, stdin);

        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)){
            printf("%s", cmd);
            fflush(stdout);
        }

        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        if (nl)
            *nl = '\0';

        /* Exit Command */
        if (!strcmp(cmd, "exit")){
            if (head == NULL){
                fprintf(stderr, "Bye...\n");
                printBuiltInMessage(cmd, SUCCESS);
                break;
            }

            fprintf(stderr, "Error: active jobs still running\n");
            checkAllBackground(&head);
            printBuiltInMessage(cmd, FAILURE);
            continue;
        }
        if (!strcmp(cmd, "pwd")){
            pwdCommand();
            checkAllBackground(&head);
            printBuiltInMessage(cmd, SUCCESS);
            continue;
        }
        /* Parsing cmd */

        char **args = getArgs(cmd);
        if (!args){
            checkAllBackground(&head);
            continue;
        }

        if (!strcmp(args[0], "cd")){
            int cdVal = cdCommand(args[1]);
            freeArgs(args);
            checkAllBackground(&head);
            printBuiltInMessage(cmd, cdVal);
            continue;
        }

        struct Job *job = malloc(sizeof(struct Job));
        InitializeJob(job);
        findAllSpecialCharLocations(args, job);

        if (checkParsingError(args, job)){
            checkAllBackground(&head);
            freeArgs(args);
            free(job);
            continue;
        }

        /* Regular command */
        freeAllSpecialChars(args, job);
        int *status = executeCommands(cmd, args, job, &head);
        checkAllBackground(&head);

        if (status != NULL){
            printMessage(cmd, status, job->numCommands);
            free(status);
        }

        freeArgs(args);
        free(job);
    }

    return EXIT_SUCCESS;
}