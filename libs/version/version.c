#include <phx/version/version.h>

#include <stdio.h>

void PHX_Print_Version(void)
{
    FILE* const stream = stdout;

    fputs(
        "PHX version " VERSION " (https://github.com/JonathanMohr/PHX)\n"
        "Compiled on " __DATE__ "\n",
        stream
    );
}
