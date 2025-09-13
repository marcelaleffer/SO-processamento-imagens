#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pgm_utils.h"
using namespace std;

const char* FIFO_PATH = "/tmp/imgpipe";

int main(int argc, char** argv) {
    if (argc < 2) {
       cerr << "Uso: " << argv[0] << " <imagem_entrada.pgm>" <<endl;
        return -1;
    }

    const char* inpath = argv[1];

    // Cria o FIFO se não existir
    mkfifo(FIFO_PATH, 0666);

    // Abre FIFO para escrita
    int fifo = open(FIFO_PATH, O_WRONLY);
    if (fifo == -1) {
        cerr << "Erro ao abrir FIFO!" << endl;
        return -1;
    }

    // Lê a imagem PGM
    PGM img;
    if (read_pgm(inpath, &img) != 0) {
       cerr << "Erro ao ler a imagem!" << endl;
        return -1;
    }

   cout << "Sender: largura=" << img.w << " altura=" << img.h << " maxv=" << img.maxv << endl;

    // Envia metadados
    write(fifo, &img.w, sizeof(img.w));
    write(fifo, &img.h, sizeof(img.h));
    write(fifo, &img.maxv, sizeof(img.maxv));

    cout << "Sender: escrevendo metadados -> largura=" << img.w
          << ", altura=" << img.h << ", maxv=" << img.maxv <<endl;


    // Envia dados dos pixels
    write(fifo, img.data, img.w * img.h * sizeof(unsigned char));

    cout << "Imagem enviada pelo FIFO com sucesso!" << endl;

    // Fecha FIFO e libera memória
    close(fifo);
    free(img.data);

    return 0;
}
