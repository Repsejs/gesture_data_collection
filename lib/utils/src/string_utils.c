#include "string_utils.h"
#include "queue.h"
#include <string.h>

int int_to_string(char *string, int integer) {
    int i = 0;
    int divider = 1000;

    if (integer < 0) {
        string[i++] = '-';
        integer = -integer;
    } else if (integer == 0) {
        string[i++] = '0';
        string[i++] = ';';
        string[i] = '\0';
        return i;
    }

    int og_num = integer;
    while (integer >= divider * 10) {
        divider *= 10;
    }

    while (divider) {
        char digit = (integer/divider) + '0';
        if (digit != '0' || og_num > integer) {
            string[i++] = digit;
        }

        integer %= divider;
        divider /= 10;
    }

    string[i++] = ';';
    string[i] = '\0';
    return i;
}

int append_int_to_string(char *string, int integer) {
    char temp[20] = {'\0'};
    int_to_string(temp, integer);
    int len = fuseStrings(string, temp);
    return len;
}

void append_newLine(char *string, int length) {
    string[length-1] = '\n';
    string[length] = '\0';
}
