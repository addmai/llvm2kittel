#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int factorial(int n) {
    int fact = 1;
    for(int i = 1; i <= n; i++) {
        fact *= i;
    }
    
    return fact;
}

int b(n) {
    return factorial(n) + 5;
}

int a(n) {
    return factorial(n) + b(n);
}

int main() {
    int n;
    printf("Enter a number: ");
    scanf("%d", &n);

    // Calculate factorial
    if (n > 0) {
        printf("Factorial of %d is: ", n);
        printf("%d\n", a(n));
    } else {
        printf("Invalid number.\n");
    }

    return 0;
}
