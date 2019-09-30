#include <stdio.h>

int f0() {
    return 0;
}

int f1() {
    return 1;
}

int f2() {
    return 2;
}

int main()
{
    int (*a[3])() = {&f0, &f1, &f2};
    printf("%d\n", a[0]());
    printf("%d\n", a[1]());
    printf("%d\n", a[2]());
}
