# SSHELL: Simple Shell

# Introduction: #

The purpose of this project is to understand how a operating system makes 
various system calls by implementing a simple shell: *sshell*. A shell is also
known as a command line-interpreter, and it takes in arguments from the user 
that the shell will execute.

Below is a simple example of a command that a shell can execute, and the 
corresponding completion message:
```bash
sshell@ucd$ echo Hello World  
Hello World  
+ completed 'echo Hello World' [0]  
```
# Features: 
1. Execution of user-supplied commands with arguments
2. Built-in commands(cd, pwd, exit)
3. Output redirection to a file
4. Piping of multiple commands
5. Output redirection appending
6. Background Processes

# Implementation
The implementation of this program can be broken down into 5 distinct steps
1. Parsing the command(user-provided input)
2. Executing built-in commands
3. Checking for parsing errors
4. Execution of other commands with piping and/or output redirection
5. Execution of background processes

# Parsing the command

After getting the user input, the shell calls the getArgs() function to parse
the command, and returns an array of strings (char * args[]). The main helper
function that is responsible for this functionality is the parseCmd() function.

The `parseCmd()` function essentially iterates through the cmd string character
by character. If any of these characters are encountered (' ', '<' , '|', '&' ),
the function will check if the length of the current word is greater than zero.
If the word length (arg_length) is greater than zero, then the word will be 
added to the next index in the args array. This done by dynamically allocating
space using malloc, and then using strcpy to copy the bytes into `args[i]` where
i = the current index of the args array. In addition if any "meta" character is
found, they will be added to the next index in the array. Arg_length will be
set to 0, and this process will continue until the end of the cmd string is
reached.
```
cmd = " echo Hello World|grep Hello<file
Index:   0        1       2        3     4      5        6     7
Args:["echo",  "Hello", "World",  "|", "grep", "Hello", "<", "file" ]
```
**Note:**
Creating the args array is only done after checking if the command is pwd or
exit because there is no need to initialize an array when there is only 
one word/command.

# Built-in Commands:

**exit:**
If the background process linked list (details found in the background process 
section) is not NULL, an error message will be printed. This is because the 
shell can only terminate when no background processes are running. The shell
will successfully terminate when the linked list is NULL by breaking from the 
infinite loop. 

**pwd:**
The pwd command prints the name of the current working directory. To implement 
this part we created a helper function called `pwdCommand()`. All that was done 
for this command was call the function `getcwd()`. This function is a syscall 
that places the name of the current directory into a string (`char *`). The string 
is printed after receiving the name of the directory.

**cd:**
The cd command allows for the user to change their current working directory. To
implement ths part we created a helper function called `cdCommand()`. Essentially
what this function does is call the function chdir() with the new directory
passed in as an argument. If `chdir()` returns 1, it means that the directory 
provided does not exist. As a result, we print an error message to indicate this
problem. If chdir() returns 0, it changed the directory with no problems.

# Parsing Errors

**struct Job:**
This data structure is relevant for the next three parts, but it is being 
introduced here because it is first used when checking for parsing errors.
Essentially what this struct stores is the location of all the meta-characters
in the args array as well the number of commands. The values of the members will
be -1 if that specific meta character is not found.

Example: 
```
         0        1        2      3     4        5      6
args: ["Echo", "Hello", "World", "|", "grep", "Hello", "&"]
job->pipes = [3, -1, -1]; job->background = 6; job->append = -1; 
job->redirect = -1; job->numCommands = 2;
```

**checking for parsing errors:**
Function checkParsingError() is the "main" function for checking for these kind
of errors, it calls helper functions to determine if the input is invalid.
All of the helpers and the main will return FAILURE(1) to indicate there is a
parsing error or SUCCESS (0) 

Function checkAtBetweenPipes() checks for errors if there are pipes. For
example, if there is a & before a |, that is an invalid command because &
can only be the last command. This is done by iterating through the values in 
the job->pipes array and comparing them to the other values in the job struct.

Function openableFile() checks if a file is openable given that the input
has asked for output redirecton. It uses the open() function, to see if the
file can be written to; open() will return -1 if the specified arguments to the
function cannot be performed.

# Regular commands with piping and/or output redirection
The function that implements the regular commands is the executeCommands() 
function.

**Note:**
Prior to calling this function, all the meta characters in the args array 
are set to NULL and freed by the function freeAllSpecialChars(). This is because
meta characters are not passed in as arguments for execvp().

**simple commands:**
For a command that does not have piping or ouput redirection, the command to be
executed will be located at the first index of the args array, and the array
contains all the remaining arguments. So, the function simply forks once
and runs excevp(args[0], args) in the child process. The exit value is retrieved
using the helper function getStatus that uses waitpid() in its blocking form
to retreive the execute value of the given command. If execvp fails, an error 
message will be printed, and the child will exit with a value of 1.

**output redirection:**
There is a helper function checkOuputRedirect() to determine if the output of 
the command should be redirected to a file. If job->redirect != -1 or 
job->append != -1, then a new file descriptor will be opened by the open() 
function. For the open function, the truncate, create, and write only flags are 
used if the < character is specified. If the << operator is specified then 
instead of the truncate flag, the append flag is used instead. After creating 
the new file descriptor, dup2() is used in order to change standard output to 
the file. The newly created file descriptor is closed after calling dup2.

**piping:**
For piping we created three int fd[2] arrays, and called the pipe syscall 
on each of these arrays. It is necessary to fork the number of times as there
are number of commands. For example, if there are two pipes then it is required
to fork three times. For each of the child processes, the relevant pipes have
to be opened and closed. The function uses the job->numCommands to determine the
number of times the function has to fork. If it is the lastCommand in the job,
then the checkOutputRedirect() function is called in the respective child
process, and dup2(pipex[1], STDOUT_FILENO) will not be called. 
The second child's execvp call will look like: 
execvp(args[job->pipes[0] + 1], &args[job->pipes[0] + 1]);

job->pipes[0] + 1 is the location after the first pipe. If there are 3 or 4 
execvp calls, it will look similar to the line of code above.

# Background Processes

**struct BackgroundProcess:**
This is a linked list that contains the following members: pids array, status
array, the user inputed cmd string, the number of commands, and a pointer to 
the next background process.

If job->background != -1, then the a new BackgroundProcess struct will be
initialize by the newBackground() function. This function will use memcpy to
copy the pids and the status array into the respective members of this struct.
The addNewBackround() function will add the new "node" to the linked list by
traversing through the list until the next pointer of a specific background
process is NULL.

The checkBackground() function is used to check if a Background Process has 
completed. The completion of a background process is checked by traversing
through the linked list and calling the checkCompletion() function. This
function calls waitpid() using WNOHANG, so it is non-blocking. If a command
in the process has finished, the pid for that command is set to -1. Once all the
commands within a process are -1 that means the background process has 
completed, and the exit status is printed. If a process has completed, then the 
removeBackground() function is called. This function will replace the head to 
head->next if the first node is removed, or it will set the previous->next = 
removed->next.

The checkbackground() function is run before the command completion
message is printed or after there is a parsing error detected. This is done
beacuse the shell prints the completion message of a background job before the 
completion message of a foreground process.


