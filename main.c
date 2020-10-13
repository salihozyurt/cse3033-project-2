#include "console.h"

void handle_ctrl_z_signal(int signal)
{
    if(foreground_pid!=0){
        kill(foreground_pid, SIGKILL);
    }else{
        printf("There is nothing to kill.");
    }
    fflush(stdout);
    printf("\nmyshell: ");
    puts("Foreground processes terminated.");
}

void handle_ctrl_c_signal(int signal)
{
    puts("");
    exit(EXIT_SUCCESS);
}

int main(int argc, char const *argv[])
{
    char input_buffer[MAX_INPUT_LIMIT];
    char *args[MAX_ARGUMENT_LIMIT];
    Boolean background;
    int status;
    printf("%s", "Initialising program: [Creating shell..]\n");
    init_standard_io();
    signal(SIGTSTP, handle_ctrl_z_signal);
    signal(SIGINT, handle_ctrl_c_signal);
    while (TRUE) 
    {
        background = __False;
        printf("%s", "myshell: ");
        setup(input_buffer, args, &background);
    }
    return EXIT_SUCCESS;
}
