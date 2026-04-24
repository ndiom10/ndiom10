#include <stdio.h>
#include <string.h>

extern int check_passphrase(const char *guess);
extern long measure_once(int *result, const char *guess,
                         int (*fn)(const char *));

void find_passphrase(char *buffer, int length) {
    char letters[] = "abcdefghijklmnopqrstuvwxyz";
    int result;

    memset(buffer, 0, length + 1);

    for (int i = 0; i < length; i++) {
        long best_time = -1;
        char best_char = 'a';

        for (int j = 0; j < 26; j++) {
            buffer[i] = letters[j];
            buffer[i + 1] = '\0';

            long total = 0;

            /* repeat timings to reduce noise */
            for (int k = 0; k < 8; k++) {
                total += measure_once(&result, buffer, check_passphrase);
            }

            if (total > best_time) {
                best_time = total;
                best_char = letters[j];
            }
        }

        buffer[i] = best_char;
    }

    buffer[length] = '\0';

    /* final confirmation */
    check_passphrase(buffer);
}
