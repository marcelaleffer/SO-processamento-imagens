#include "pgm_utils.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

int read_pgm(const char* path, PGM* img) {
    FILE* file = fopen(path, "rb");
    if (!file) return -1;

    // Ler cabeçalho
    char format[3];
    fscanf(file, "%2s", format); // P5
    if (strcmp(format, "P5") != 0) {
        fclose(file);
        return -2;
    }

    // Ignora comentários
    char c;
    while ((c = fgetc(file)) == '#') {
        while (fgetc(file) != '\n');
    }
    ungetc(c, file);

    fscanf(file, "%d %d\n%d\n", &img->w, &img->h, &img->maxv);

    img->data = (unsigned char*)malloc(img->w * img->h * sizeof(unsigned char));
    fread(img->data, sizeof(unsigned char), img->w * img->h, file);

    fclose(file);
    return 0;
}

int write_pgm(const char* path, const PGM* img) {
    FILE* file = fopen(path, "wb");
    if (!file) return -1;

    fprintf(file, "P5\n%d %d\n%d\n", img->w, img->h, img->maxv);
    fwrite(img->data, sizeof(unsigned char), img->w * img->h, file);

    fclose(file);
    return 0;
}
