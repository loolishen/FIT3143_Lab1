
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>


bool main(int k)
{
    if (k < 0) {
        return false;
    }
    if (k == 0) {
        return true;
    }
    if (k % 2 == 0) {
        return false;
    }

    int limit = sqrt(k);
        for (int i = 3; i <= limit; i += 2) {
            if (k % i == 0) {
                return false;
            }
        }
        return true;

}

