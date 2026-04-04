#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <omp.h>

// v1
double geomean_v1(unsigned char *s, size_t n) {
    double answer = 0;
    #pragma omp parallel for reduction(+:answer)
    for (int i = 0; i < n; i++) {
        if (s[i] > 0) answer += log(s[i]) / n;
    }
    return exp(answer);
}

// v2
double geomean_v2(unsigned char *s, size_t n) {
    double answer = 0;
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        if (s[i] > 0) {
            double val = log(s[i]) / n;
            #pragma omp atomic
            answer += val;
        }
    }
    return exp(answer);
}

// v3
double geomean_v3(unsigned char *s, size_t n) {
    double answer = 0;
    #pragma omp parallel
    {
        double local = 0;
        #pragma omp for nowait
        for (int i = 0; i < n; i++) {
            if (s[i] > 0) local += log(s[i]) / n;
        }
        #pragma omp atomic
        answer += local;
    }
    return exp(answer);
}

// v4
double geomean_v4(unsigned char *s, size_t n) {
    double answer = 0;
    #pragma omp parallel for schedule(dynamic) reduction(+:answer)
    for (int i = 0; i < n; i++) {
        if (s[i] > 0) answer += log(s[i]) / n;
    }
    return exp(answer);
}

// v5
double geomean_v5(unsigned char *s, size_t n) {
    double answer = 0;
    int j = 0;
    #pragma omp parallel reduction(+:answer)
    while (1) {
        int i;
        #pragma omp atomic capture
        i = j++;
        if (i >= n) break;
        if (s[i] > 0) answer += log(s[i]) / n;
    }
    return exp(answer);
}

long long nsecs() {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return t.tv_sec*1000000000 + t.tv_nsec;
}

int main(int argc, char *argv[]) {
    char *s = NULL;
    size_t n = 0;

    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size_t size = ftell(f);
            fseek(f, 0, SEEK_SET);
            s = realloc(s, n + size);
            fread(s + n, 1, size, f);
            fclose(f);
            n += size;
        }
    }

    if (n == 0) {
        printf("No input data\n");
        return 1;
    }

    double (*geomean_func)(unsigned char*, size_t) = geomean_v1;

    long long t0 = nsecs();
    double answer = geomean_func((unsigned char*)s, n);
    long long t1 = nsecs();

    printf("%lld ns to process %zu characters: %g\n", t1 - t0, n, answer);

    free(s);
    return 0;
}
