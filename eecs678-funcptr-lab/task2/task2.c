#include <stdio.h>
#include <stdlib.h>

#define A 6
#define B 3

/* IMPLEMENT ME: Declare your functions here */
int add (int a, int b);
int subtract (int a, int b);
int multiply (int a, int b);
int divide (int a, int b);

typedef int (*num_func) (int a, int b);

int main (void)
{

	num_func func_array[4] = {add, subtract, multiply, divide};

	printf("Operand 'a' : %d | Operand 'b' : %d\n", A, B);
	printf("Specify the operation to perform (0 : add | 1 : subtract | 2 : Multiply | 3 : divide): ");

	int result = func_array[0](A, B);

	printf("\nx = %d\n", result);

	return 0;
}

/* IMPLEMENT ME: Define your functions here */
int add (int a, int b) { printf ("Adding 'a' and 'b'\n"); return a + b; }
int subtract (int a, int b) { printf ("Subtracting 'b' from 'a'\n"); return a - b; }
int multiply (int a, int b) { printf ("Multiplying 'a' and 'b'\n"); return a * b; }
int divide (int a, int b) { printf ("Dividing 'a' by 'b'\n"); return a / b; }
