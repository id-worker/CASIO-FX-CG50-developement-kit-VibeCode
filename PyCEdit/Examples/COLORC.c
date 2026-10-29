/* PyCEdit: C syntax highlighting test.
 * Open this .c file and scroll through it.
 * This is a multiline comment: "text" and 42
 * should stay comment-colored until the closing mark.
 */

/* PicoC has printf built in; C compilers need stdio. */
#ifdef __STDC__
#include <stdio.h>
#endif

#define COUNT 8
#define LOW 15
#define HIGH 24

int readings[COUNT] = {18, 23, 12, 27, 20, 14, 25, 19};

char *classify(int value)
{
    if (value < LOW) {
        return "LOW";
    } else if (value > HIGH) {
        return "HIGH";
    }
    return "OK";
}

int sum_values(int *values, int count)
{
    int total = 0;
    int i;

    for (i = 0; i < count; i++) {
        total = total + values[i];
    }
    return total;
}

int count_ok(int *values, int count)
{
    int result = 0;
    int i;

    for (i = 0; i < count; i++) {
        int value = values[i];
        if (value >= LOW && value <= HIGH) {
            result = result + 1;
        }
    }
    return result;
}

void show_readings(void)
{
    int i;
    printf("Sensor report\n");
    printf("------------------------\n");

    for (i = 0; i < COUNT; i++) {
        printf("Reading %d: %d - %s\n", i + 1,
               readings[i], classify(readings[i]));
    }

    printf("Total: %d\n", sum_values(readings, COUNT));
    printf("In range: %d\n", count_ok(readings, COUNT));
}

void show_strings(void)
{
    // Symbols inside quotes are not comments.
    printf("A # here is just text\n");
    printf("C-like // symbols are just text\n");
    printf("Quoted word: \"hello\"\n");
    printf("Hex number: %d\n", 0x2A);
}

int main(void)
{
    show_readings();
    show_strings();
    printf("Done!\n");
    return 0;
}

/* PicoC script mode does not call main automatically. */
#ifndef __STDC__
main();
#endif
