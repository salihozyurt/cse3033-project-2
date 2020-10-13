// Muhammed Bera KOÇ 150116062
// Salih ÖZYURT 150117855

#if !defined(CONSOLE_H)
#define CONSOLE_H

// İnclude builtin libraries
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

// Define macros to be used later
#define MAX_INPUT_LIMIT 128
#define MAX_ARGUMENT_LIMIT 32
#define BIG_SIZE 1024
#define DEFAULT_SIZE 256
#define SMALL_SIZE 128

// __History char array holds the past record of written terminal commands
char *__History[10];
// Holds the index value for the current history
int history_index = 0;

char *__commandOfDoubleCommand;

// Pseudo Boolean enum to make things easy
typedef enum 
{
    FALSE, TRUE
} Boolean;

typedef struct ParserNode ParserNode;
typedef ParserNode * ParserNodePtr;
// A struct which hold two values an array of strings and the length of array
struct ParserNode
{
    char **capsule;
    int length;
};
typedef struct childProcess
{
    pid_t pid;
    struct childProcess *next;
}childProcess;

childProcess *child_head = NULL;
pid_t foreground_pid;
int number_of_child_processes = 0;
Boolean is_double_command = FALSE;

// Creator of ParserNode struct
ParserNode _ParserNode(char *capsule[], int length)
{
    ParserNodePtr parser_node_ptr = (ParserNodePtr) malloc(sizeof(ParserNode));
    if (parser_node_ptr != NULL)
    {
        parser_node_ptr->capsule = capsule;
        parser_node_ptr->length = length;
        return *parser_node_ptr;
    }
    else
    {
        _ParserNode(capsule, length);
    }
}

// Two main parser node one for system path variable other for current argument
ParserNode pathNode, argsNode;
// Boolean values of different IO operations
Boolean Write, WriteAppend, WriteError, Read;
// Standard IO devices
int protected_stdout, protected_stdin, protected_stderr;
// Directed IO devices
int formatted_stdout, formatted_stdin, formatted_stderr;

// Initiliases standard IO devices by storing them in protected file descriptors
void init_standard_io()
{
    protected_stdout = dup(STDOUT_FILENO);
    protected_stdin = dup(STDIN_FILENO);
    protected_stderr = dup(STDERR_FILENO);
}

// A function which takes an array of strings, a string and a delimeter
// Splits string using delimeter and puts substrings intor capsule.
// Also returns a ParserNode which holds the length of capsule
ParserNode parse_string(char *capsule[], char input[], const char delim[])
{
    int index = 0; // An index variable to hold the current substring location
    char *token; // Creates a token and stores each substing inside this token
    token = strtok(input, delim);
    while (token != NULL)
    {
        capsule[index++] = token; // puts the token inside capsule increasing index by one
        token = strtok(NULL, delim); // In each iteration splits one chunk of string
    }
    return _ParserNode(capsule, index); // Returns a ParserNode using creator function
}

void setup(char input_buffer[], char *args[], Boolean *background);

// Initialises terminal history and sets it for each command
void initHistory(char *input)
{
    if (history_index != 10) // If total commands are less than ten it will work like a stack
    {
        if(history_index == 0)
        {
            __History[history_index] = input;
        }
        else
        {
            for (int i = history_index; i > 0; i--)
            {
                __History[i] = __History[i-1]; // Shift right operation
            }
            __History[0] = input; 
        }
        history_index++;
    }
    else // Otherwise function will not increase index since stack is full
    {
        for (int i = history_index; i > 0; i--)
            {
                __History[i] = __History[i-1];
            }
        __History[0] = input;
    }  
}

// Prints each command in history until history index
void writeHistory()
{
    for (int i = 0; i < history_index; i++)
    {
        printf("\t%d %s\n",i,__History[i]);
    }
}

// Takes an exec_name(e.g.: ls) and returns the full path of the given command
char * get_exec_path(char *exec_name)
{
    int file_descriptor; // A file descriptor for path file
    int standard_output = dup(STDOUT_FILENO); // Store standard output device in a variable
    // For flags: https://www.gnu.org/software/libc/manual/html_node/Open_002dtime-Flags.html
    if ((file_descriptor = open("path.log", O_CREAT|O_TRUNC|O_WRONLY, 0777)) < 0) 
    {   // Creates a file to store the full path and controls if there is any error
		perror("Cannot open file descriptor.");	/* open failed */
		exit(1);
	}
    fflush(stdout); // Forces the terminal to flush its buffer
    if (dup2(file_descriptor, STDOUT_FILENO) < 0) // Makes standard output device given fd
    {
        perror("Cannot reset output device.");
        return "FAILURE";
    }
    if (fork() == 0) // Creates a new child for execv
    {
        // Using whereis with full path finds the command path and writes it into paht.log
        execv("/usr/bin/whereis", (char*[]){"/usr/bin/whereis", exec_name, NULL});
        exit(EXIT_SUCCESS);
    }
    else
    {
        wait(NULL);
    }
    if (dup2(standard_output, STDOUT_FILENO) < 0) // Resets standard output device
    {
        perror("Cannot reset output device.");
        return "FAILURE";
    }
    // Formats the file data and converts it the full path
    // Since whereis output is a little bit messy
    char *data = malloc(DEFAULT_SIZE);
    FILE *path_file_ptr = fopen("path.log", "r");
    // This will be like -> command:
    fscanf(path_file_ptr, "%s", data);
    // If second reading (full path) is -1 that means command is not found
    int result = fscanf(path_file_ptr, "%s", data);
    // -1 = EOF
    if (result == -1)
    {
        fprintf(stderr, "%s%s\n", "Cannot find command: ", exec_name);
        remove("path.log");
        return "FAILURE";
    }
    // Closes file descriptor since it is of no use now
    close(file_descriptor);
    // Removes the path file 
    remove("path.log");
    // Returns the full path
    return data;
}
// Handles cd command since it is builtin exec with no full path
void handle_cd_command(char *input_path)
{
    // if nothing is given it does nothing
    if (input_path == NULL) return;
    // Otherwise changes the direction using given path
    chdir(input_path);
}

// Path command is a new command. Henceforth it should be handled by us
// It takes two arguments: option can be + or - and directory which is new directory
// which is to be added to system path variable
void handle_path_command(char *option, char *directory)
{
    // __path variable holds the current system path
    char *__path = getenv("PATH");
    // A variable for modified system path
    char formatted_path[DEFAULT_SIZE] = "";
    // If option is + which mean adding a new path to system
    if (!strcmp(option, "+"))
    {
        // Combines new directory and current system path
        sprintf(formatted_path, "%s:%s", directory, __path);
        // Sets the new path variable with formatted path
        // Last argument means overwriting is true
        setenv("PATH", formatted_path, TRUE);
    }
    // If - it means given directory will be removed with all duplicates
    else if (!strcmp(option, "-"))
    {
        // Puts all path directories in global variable using : as a delimiter
        pathNode = parse_string((char*[DEFAULT_SIZE]){}, __path, ":");
        // Traverse between directories
        for (int i = 0; i < pathNode.length; ++i)
        {
            // When a system directory is not equal to given directory
            // Concatenates it to the formatted path
            if (strcmp(pathNode.capsule[i], directory))
            {
                strcat(formatted_path, pathNode.capsule[i]);
                strcat(formatted_path, ":"); // Adds it for default path format
            }
        }
        formatted_path[strlen(formatted_path) - 1] = '\0'; // To remove last : since its redundant
        char *resized_path = malloc(BIG_SIZE); // Creates new char array
        sprintf(resized_path, "%s", formatted_path); // Puts the formatted path into resized path
        setenv("PATH", resized_path, TRUE); // Sets the path environment variable as resized path
    }
}

typedef struct Storage Storage;
// This struct holds two ParserNode
// Very useful for commands like ls -a > input.txt
// Since there are two paths : First is pure args: [ls -a]
// Second is [> input.txt] direction commands or io commands
struct Storage
{
    ParserNode args;
    ParserNode io;
};

// Sets io signals using given command node
// Returns a storage struct
Storage set_io_signals(ParserNode argsNode)
{
    Boolean gate = TRUE; // Gate is created to detect the first IO symbol
    int io_index = 0; // The index of first io symbol => io symbols: [>, >>, 2>, <]
    // Initiliases all symbols to False
    Write = FALSE, WriteAppend = FALSE, WriteError = FALSE, Read = FALSE;
    // Loops through the commands
    for (int i = 0; i < argsNode.length; ++i)
    {
        // If current node is Null we reached the end it breaks
        if (argsNode.capsule[i] == NULL) break;
        // If current node is one of the io symbols it sets the io_index and closes the gate
        if (!strcmp(argsNode.capsule[i], ">") || !strcmp(argsNode.capsule[i], ">>") ||
            !strcmp(argsNode.capsule[i], "2>") || !strcmp(argsNode.capsule[i], "<"))
        {
            if (gate)
            {
                io_index = i;
                gate = FALSE;
            }
        }
        // Set IO signals
        if (!strcmp(argsNode.capsule[i], ">")) Write = TRUE;
        else if (!strcmp(argsNode.capsule[i], ">>")) WriteAppend = TRUE;
        else if (!strcmp(argsNode.capsule[i], "2>")) WriteError = TRUE;
        else if (!strcmp(argsNode.capsule[i], "<")) Read = TRUE;
    }
    // That means no IO symbol has been found it returns only to one ParserNode
    // Given argsNode since no IO part exists
    if (io_index == 0) return (Storage) {(ParserNode) {capsule: argsNode.capsule, length: argsNode.length}};
    // Initialises the ParserNodes
    char *current_args[DEFAULT_SIZE];
    char *current_io[DEFAULT_SIZE];
    // Until it reaches the io_index puts the strings inside the argsNode
    for (int i = 0; i < io_index; ++i)
    {
        current_args[i] = argsNode.capsule[i];
        if (i == io_index - 1) current_args[i + 1] = NULL;
    }
    // After io_index is reached. Puts all the strings in io ParserNode
    for (int i = 0; i < argsNode.length - io_index; ++i)
    {
        current_io[i] = argsNode.capsule[i + io_index];
    }
    // Returns the two ParserNodes
    return (Storage) {(ParserNode) {capsule: current_args, length: io_index + 1},
        (ParserNode) {capsule: current_io, length: argsNode.length - io_index}};
}

// Resets the IO devices to standard ones
void reset_file_descriptors()
{
    if (Write || WriteAppend) 
    {
        dup2(protected_stdout, STDOUT_FILENO);
        close(formatted_stdout);
    }
    if (Read)
    {
        dup2(protected_stdin, STDIN_FILENO);
        close(formatted_stdin);
    }
    if (WriteError)
    {
        dup2(protected_stderr, STDERR_FILENO);
        close(formatted_stderr);
    }
}

// Sets the files descriptors using io ParserNode
void set_file_descriptors(ParserNode io)
{
    // Loops through all strings
    for (int i = 0; i < io.length; ++i)
    {
        // If current is NULL we reached the end break!
        if (io.capsule[i] == NULL) break;
        // If current string is equal to >, that means it is writing without append
        if (!strcmp(io.capsule[i], ">"))
        {
            ++i; // Increase index to reach the file index
            //   i       i + 1
            // {">", "input.txt", ..., NULL}
            // Remove if the file exists
            remove(io.capsule[i]);
            // Make the output file descriptor given file 
            // 0777 means give all rights to every user
            // To prevent the need of sudo while starting the code
            if ((formatted_stdout = open(io.capsule[i], O_CREAT|O_WRONLY, 0777)) < 0)
            {
		        perror("Cannot open file descriptor.");	/* open failed */
		        exit(1);
	        }
            // Make the current standard output device current file descriptor
            if (dup2(formatted_stdout, STDOUT_FILENO) < 0)
            {
                perror("Cannot reset output device.");
                exit(1);
            }
        }
        // Same as above, >> means write with append : add 0_APPEND to activate append option
        else if (!strcmp(io.capsule[i], ">>"))
        {
            ++i;
            if ((formatted_stdout = open(io.capsule[i], O_CREAT|O_WRONLY|O_APPEND, 0777)) < 0)
            {
		        perror("Cannot open file descriptor.");
		        exit(1);
	        }
            if (dup2(formatted_stdout, STDOUT_FILENO) < 0)
            {
                perror("Cannot reset output device.");
                exit(1);
            }
        }
        // Same process 2> same as >. The only difference is for error we use standard
        // error device
        else if (!strcmp(io.capsule[i], "2>"))
        {
            ++i;
            remove(io.capsule[i]);
            if ((formatted_stdout = open(io.capsule[i], O_CREAT|O_WRONLY, 0777)) < 0)
            {
		        perror("Cannot open file descriptor.");
		        exit(1);
	        }
            if (dup2(formatted_stderr, STDERR_FILENO) < 0)
            {
                perror("Cannot reset output device.");
                exit(1);
            }
        }
        // For input (<) we use standard input device
        else if (!strcmp(io.capsule[i], "<"))
        {
            ++i;
            if ((formatted_stdin = open(io.capsule[i], O_CREAT|O_RDONLY, 0777)) < 0)
            {
		        perror("Cannot open file descriptor.");
		        exit(1);
	        }
            if (dup2(formatted_stdin, STDIN_FILENO) < 0)
            {
                perror("Cannot reset output device.");
                exit(1);
            }
        }
    }
}

Boolean __True = TRUE, __False = FALSE;

Boolean isBackground(char input[]){
    int length = strlen(input);
    for (int i = 0; i < length; i++)
    {
        if(input[i] == '&')
        {
            return __True;
        }
    }
    return __False;
}

void foreground(int process_id){
    if(child_head != NULL){
        childProcess *iter;
        pid_t pid;
        iter = child_head;
        while (iter != NULL)
        {
            if(!tcsetpgrp(getpid(), getpgid(iter->pid))){
                fprintf(stdout, "Error!");
            }else{
                foreground_pid = iter->pid;
                waitpid(iter->pid,NULL,0);
                foreground_pid = 0;
            }
            iter = iter->next;
        }
        


        
    } else{
        fprintf(stderr, "Any background process does not exist!\n");
        return;
    }
}

void __shallow_copy(char *source[], char *dest[], int length)
{
    for (int i = 0; i < length; ++i)
    {
        source[i] = dest[i];
    }
}

void doubleCommand(char input[])
{
    ParserNode commands = parse_string((char*[]){}, input,";");
    char *copied_commands[commands.length];
    __shallow_copy(copied_commands, commands.capsule, commands.length);
    char input_buff[MAX_INPUT_LIMIT];
    char *argsTemp[MAX_ARGUMENT_LIMIT];
    Boolean backGroundTemp = FALSE;
    for (int i = 0; i < commands.length; i++)
    {
        __commandOfDoubleCommand = copied_commands[i];
        setup(input_buff, argsTemp, &backGroundTemp);
    }
    is_double_command = FALSE;
}

Boolean __CommandGate;

void __isDoubleCommand(char input[]){
    int length = strlen(input);
    for (int i = 0; i < length; i++)
    {
        if(input[i] == ';')
        {
            __CommandGate = __True;
            is_double_command = TRUE;
            doubleCommand(input);
            break;
        }
    }
    is_double_command = FALSE;
}

Boolean is_child_invoked;

char * process_input(char input_buffer[], char *args[], Boolean *background)
{
    char *input;
    if (*background == __True)
    {
        is_child_invoked = __True;
        *background = __False;
        input = input_buffer;
    }
    else
    {
        if (is_double_command == FALSE)
        {
            input = fgets(input_buffer, MAX_INPUT_LIMIT, stdin);
            if (input == NULL)
            {
                puts("");
                exit(EXIT_SUCCESS);
            }
            input = strdup(input);
            __isDoubleCommand(input_buffer);
            if (__CommandGate == TRUE)
            {
                __CommandGate = __False;
                return NULL;
            }         
        } 
        else
        {
            input = strdup(__commandOfDoubleCommand);
        }
        *background = isBackground(input);
        if (input[strlen(input) - 1] == '\n') input[strlen(input) - 1] = '\0';
        
    }
    // To prevent input from manipulations use a copied input
    char *input_copy = strdup(input); 
    // Splits input into substrings using space delimiter
    argsNode = parse_string(args, input, " ");
    if(*background == __True)
    {
        argsNode.capsule[argsNode.length - 1] = NULL;
        argsNode.length = argsNode.length - 1;
    }
    // Adds command to the history if given command is not history
    if (strcmp(argsNode.capsule[0],"history"))
    {
        initHistory(strdup(input_copy));
    }
    // Adds a new cell to argsNode capsule as NULL
    // To provide the execv format
    argsNode.capsule[argsNode.length++] = NULL;
    return input_copy;
}
       
// Core function for reading terminal inputs
void setup(char input_buffer[], char *args[], Boolean *background) {
    char * input_copy = process_input(input_buffer, args, background);
    if (input_copy == NULL) return;
    // Splits args into two pure args and io args
    Storage storage = set_io_signals(argsNode);
    // Makes args the pure args
    args = storage.args.capsule;
    // Sets the io devices using io args
    set_file_descriptors(storage.io);
    // If command is c handle it
    if (!strcmp(args[0], "cd"))
    {
        handle_cd_command(args[1]);
    }
    // If command is path handle it
    else if (!strcmp(args[0], "path"))
    {
        // Means if there is only one argument "path"
        if (storage.args.length == 2) puts(getenv("PATH"));
        else handle_path_command(args[1], args[2]);
    }
    // If given command is exit 
    else if (!(strcmp(args[0], "exit")))
    {
        exit(EXIT_SUCCESS);
    }
    else if (!(strcmp(args[0], "fg")))
    {
        foreground(atoi(args[1]));
    }
    // For other commands normal execution
    else 
    {
        // If command is history
        if (!(strcmp(args[0], "history")))
        {
            // If history is given with -i alias
            if (storage.args.length == 4)
            {
                // Obtain the ith command parse it and put it inside args
                args = parse_string((char*[DEFAULT_SIZE]){ },__History[atoi(args[2])]," ").capsule;
            }
            // if there is only history without no args
            else
            {
                // Print the history
                writeHistory();
                // Resets the IO devices
                reset_file_descriptors();
                // Finishes the current command
                return;
            }     
        } 
        // If given command is not in form ./blahblah (execute command)
        if (args[0][0] != '.' && args[0][1] != '/')
        {
            // Find the full path of command
            char *exec_path = get_exec_path(args[0]);
            args[0] = exec_path;
        }
        pid_t child_id;
        if ((child_id = fork()) == 0)
        {
            if (*background == __True)
            {
                input_copy[strlen(input_copy) - 1] = '\0';
                setup(input_copy, args, background);
            }
            else
            {
                execv(args[0], args);
                exit(EXIT_SUCCESS);
            }
        }
        else
        {
            if (*background == __True)
            {
                ++number_of_child_processes;
                printf("[%d] %d \n", number_of_child_processes, child_id);
                childProcess child_process = (childProcess) {pid: child_id, next: NULL};
                childProcess *iter;
                if(child_head == NULL)
                {
                    child_head = &child_process;
                }
                else
                {
                    iter = child_head;
                    while (iter->next != NULL)
                    {
                        iter = iter->next;
                    }
                    iter->next = &child_process;                
                }
            }
            else
            {
                wait(NULL);
            }
        }
        reset_file_descriptors();
    }
}
#endif // SETUP_H
