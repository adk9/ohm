#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static unsigned int count;
static size_t limit;

static int _incr(void) {
    sleep(2);
    return count++;
}

static int _decr(void) {
    sleep(2);
    return count--;
}

int main(int argc, char* argv[])
{
    limit = 60;
    if (argc > 1)
        limit = atoi(argv[1]);
  
    printf("Counting demo starting with pid %d\n", (int)getpid());

    for (count = 0; count < limit;)
        printf("Count = %d\n", _incr());

    for (count >= limit; count > 0;)
        printf("Count = %d\n", _decr());

    return 0;
}
