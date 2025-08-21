#include <stdio.h>

int fct (int n, int tmp);

int fct(int n, int tmp) {
    if(n == 0) {
        return tmp;
    }

    return fct(n - 1, n * tmp);
}

int factorial(int n) {
    return fct(n, 1);
}

int main() {
    int n;
    printf("Enter a number: ");
    scanf("%d", &n);

    // Calculate factorial
    if (n > 0) {
        printf("Factorial of %d is: ", n);
        printf("%d\n", factorial(n));
    } else {
        printf("Invalid number.\n");
    }

    return 0;
}
