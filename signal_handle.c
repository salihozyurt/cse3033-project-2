#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void handle(int __sig_num)
{
    puts("You pressed Control+C.");
    exit(0);
}

int main(int argc, char const *argv[])
{
    signal(SIGINT, handle);
    while (1)
    {
        printf("%s\n", "Just rolling baby.");
    }
    return 0;
}
