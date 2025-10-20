#include <instrumentation_control.h>
#include <omp.h>

int __attribute__((noinline)) IterativeFactorial(int x) {
    if (x == 0 || x == 1) {
        return 1;
    }

    int res = 1;
    for (int i = 2; i < x; ++i) {
        res *= i;
    }

    return res;
}

int __attribute__((noinline)) RecursiveFactorial(int x) {
    if (x == 0) {
        return 1;
    }

    return x * RecursiveFactorial(x - 1);
}

int main(void) {
    omp_set_nested(1);
#pragma omp parallel
    {
        BeginInstrumentationBlock();
        EnableThreadInstrumentation();

        volatile int a = 5;
        volatile int b = 5;
#pragma omp parallel
       {
#pragma omp critical
            {
                a = IterativeFactorial(a);
            }
        }
#pragma omp critical
        {
            b = RecursiveFactorial(b);
        }

        DisableThreadInstrumentation();
        EndInstrumentationBlock();
    }

    return 0;
}