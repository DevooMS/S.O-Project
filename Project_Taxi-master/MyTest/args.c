#include <stdio.h>
#include <stdlib.h>

int main() {
char *names[] = { "Huey", "Dewey", "Louie", "Donald Duck"};
void *ptr;
ptr = *(names+2);

printf("%s ", (char *)ptr );
printf("%s",  (char *)ptr+2 );
}