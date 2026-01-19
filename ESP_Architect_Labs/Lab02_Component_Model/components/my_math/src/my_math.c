#include "my_math.h"
#include "sdkconfig.h"

int My_add(int a,int b)
{
    #ifdef CONFIG_MY_MATH_ADD
        return a+b;
    #endif
        return 1;
}

int My_multiply(int a,int b)
{
    #ifdef CONFIG_MY_MATH_MULTIPLY
        return a*b;
    #endif
        return 1;
}

