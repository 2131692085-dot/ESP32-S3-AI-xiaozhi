#include <stdio.h>
#include "hello.h"
#include "hello2.h"

void func(void)
{
   printf("hello\n");
   func2();
}
