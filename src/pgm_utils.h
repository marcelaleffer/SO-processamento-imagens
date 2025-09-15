#ifndef PGM_UTILS_H
#define PGM_UTILS_H

#include <cstdio>
#include <cstdlib>
#include <iostream>

// Estrutura da imagem PGM
struct PGM {
    int w, h, maxv;
    unsigned char* data;
};

// Estrutura do cabeçalho (opcional, caso queira enviar metadados)
/*struct Header {
    int w, h, maxv;
    int mode; // 0 = NEGATIVO, 1 = SLICE
    int t1, t2;
};*/

// Funções para leitura e escrita de PGM
int read_pgm(const char* path, PGM* img);
int write_pgm(const char* path, const PGM* img);

#endif
