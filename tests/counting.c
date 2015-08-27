#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct counter_t counter_t;
struct counter_t {
    unsigned int count;
    size_t limit;
};

static counter_t ctr;

int main(int argc, char* argv[])
{
    ctr.limit = 120;
    if (argc > 1)
        ctr.limit = atoi(argv[1]);
  
    printf("Counting demo starting with pid %d\n", (int)getpid());

    int i;
    for (i = 0; i < ctr.limit; ++i) {
        printf("Count = %d\n", ctr.count);
        sleep(2);
        ++ctr.count;
    }
    return 0;
}
