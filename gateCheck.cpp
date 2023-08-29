int gateCounter = 0;
int steps = 12;
int gateOpen = 1350;
int gateClosed = 1625;
int degreesPerStep = (gateOpen - gateClosed) / steps;
#include <stdio.h>
int main() {
	for(int i = 0; i < 5; i++) {
    printf("%d\n\n", i);
    while(gateCounter < steps) {
        printf("%d\n", (gateOpen - (gateCounter++ + 1) * degreesPerStep));
    }
    gateCounter = 0;
    printf("%c%c", '\n', '\n');
    }
    return 0;
}