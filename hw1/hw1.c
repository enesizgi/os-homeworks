#include <stdio.h>
#include "parser.h"

int main()
{
    // get a single line from stdin
    while (1)
    {
        char line[256];
        fgets(line, 256, stdin);
        // parse the line
        // compare line with string 'quit' and quit if it matches
        if (strcmp(line, "quit\n") == 0)
            exit(0);

        printf("%s", line);
    }
    return 0;
}