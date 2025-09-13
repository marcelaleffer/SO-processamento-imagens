#include <iostream>     // Para cout/cerr
#include <pthread.h>    // Para threads
#include <fcntl.h>      // Para open()
#include <sys/stat.h>   // Para mkfifo()
#include <unistd.h>     // Para read() e close()
#include <cstring>      // Para strcmp()
#include "pgm_utils.h"  // Estruturas e funções para PGM
#include "filters.h"    // Funções de filtro negativo e slice

// Número de threads que vamos usar
const int NUM_THREADS = 4;

// Estruturas globais para armazenar imagem de entrada e saída
PGM g_in, g_out;

// Modo do filtro (0 = NEGATIVO, 1 = SLICE)
int g_mode;
int g_t1, g_t2;  // Limites do slice (apenas se g_mode = 1)

// Estrutura para passar argumentos para cada thread
struct ThreadArgs {
    int start_row;  // Linha inicial do bloco
    int end_row;    // Linha final do bloco (exclusiva)
};

// Função que cada thread vai executar
void* process_lines(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg; // Converte o ponteiro genérico para ThreadArgs
    int start = args->start_row;         // Pega a linha inicial
    int end = args->end_row;             // Pega a linha final

    if (g_mode == 0) {
        // Aplica o filtro negativo nesse bloco de linhas
        apply_negative(&g_in, &g_out, start, end);
    } else {
        // Aplica o filtro slice nesse bloco de linhas
        apply_slice(&g_in, &g_out, start, end, g_t1, g_t2);
    }

    return nullptr; // Fim da thread
}

int main(int argc, char** argv) {
    // Verifica se os argumentos foram passados corretamente
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <saida.pgm> <negativo|slice> [t1 t2]" << std::endl;
        return -1;
    }

    const char* outpath = argv[1];  // Caminho do arquivo de saída
    const char* mode = argv[2];     // Tipo de filtro

    // Define o modo do filtro
    if (strcmp(mode, "negativo") == 0) g_mode = 0;
    else if (strcmp(mode, "slice") == 0) {
        g_mode = 1;
        if (argc < 5) { // Slice precisa dos limites t1 e t2
            std::cerr << "Para slice, informe t1 e t2!" << std::endl;
            return -1;
        }
        g_t1 = atoi(argv[3]); // Converte argumento string para inteiro
        g_t2 = atoi(argv[4]);
    } else { // Modo inválido
        std::cerr << "Modo inválido!" << std::endl;
        return -1;
    }

    // Cria o FIFO no Linux, se ainda não existir
    mkfifo("/tmp/imgpipe", 0666);

    // Abre o FIFO para leitura (bloqueia até o sender escrever)
    /*int fifo = open("/tmp/imgpipe", O_RDWR);
    if (fifo == -1) {
        perror("Erro ao abrir FIFO");
        return -1;
    }*/
    int fifo = open("/tmp/imgpipe", O_RDONLY);
    if (fifo == -1) { perror("Erro ao abrir FIFO"); return -1; }

    // Para bloquear até o sender escrever
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fifo, &rfds);
    select(fifo + 1, &rfds, NULL, NULL, NULL);

    std::cout << "Worker: metadados recebidos -> largura=" << g_in.w
          << ", altura=" << g_in.h << ", maxv=" << g_in.maxv << std::endl;

    // Lê metadados da imagem enviados pelo sender
    /*read(fifo, &g_in.w, sizeof(g_in.w));   // Largura
    read(fifo, &g_in.h, sizeof(g_in.h));   // Altura
    read(fifo, &g_in.maxv, sizeof(g_in.maxv)); // Valor máximo do pixel
    

    if (read_n_bytes(fifo, &g_in.w, sizeof(int)) != sizeof(int) ||
        read_n_bytes(fifo, &g_in.h, sizeof(int)) != sizeof(int) ||
        read_n_bytes(fifo, &g_in.maxv, sizeof(int)) != sizeof(int)) {
        fprintf(stderr, "Erro ao ler metadados do FIFO\n");
        exit(1);
    }*/

    /*// Lê metadados da imagem enviados pelo sender
    read(fifo, &g_in.w, sizeof(g_in.w));   // Largura
    read(fifo, &g_in.h, sizeof(g_in.h));   // Altura
    read(fifo, &g_in.maxv, sizeof(g_in.maxv)); // Valor máximo do pixel

    std::cout << "Worker: metadados recebidos -> largura=" << g_in.w
            << ", altura=" << g_in.h << ", maxv=" << g_in.maxv << std::endl;


    // Aloca memória para a imagem de entrada e saída
    g_in.data = (unsigned char*)malloc(g_in.w * g_in.h);   // Entrada
    g_out.data = (unsigned char*)malloc(g_in.w * g_in.h);  // Saída
*/

    // Lê metadados
    read(fifo, &g_in.w, sizeof(g_in.w));
    read(fifo, &g_in.h, sizeof(g_in.h));
    read(fifo, &g_in.maxv, sizeof(g_in.maxv));

    // >>> preencha o cabeçalho de saída <<<
    g_out.w    = g_in.w;
    g_out.h    = g_in.h;
    g_out.maxv = g_in.maxv;

    std::cout << "Worker: metadados recebidos -> largura=" << g_in.w
            << ", altura=" << g_in.h << ", maxv=" << g_in.maxv << std::endl;

    // Alocação
    g_in.data  = (unsigned char*)malloc(g_in.w * g_in.h);
    g_out.data = (unsigned char*)malloc(g_in.w * g_in.h);

    // Lê os pixels da imagem do FIFO
    //read(fifo, g_in.data, g_in.w * g_in.h * sizeof(unsigned char));
    size_t total_bytes = g_in.w * g_in.h * sizeof(unsigned char);
    size_t bytes_read = 0;

    while (bytes_read < total_bytes) {
        ssize_t r = read(fifo, g_in.data + bytes_read, total_bytes - bytes_read);
        if (r <= 0) {
            perror("Erro ao ler pixels do FIFO");
            break;
        }
        bytes_read += r;
    }

    // Fecha o FIFO, pois já lemos tudo
    close(fifo);

    std::cout << "Worker: começando processamento..." << std::endl;

    // Criar threads e dividir a imagem em blocos
    pthread_t threads[NUM_THREADS];
    ThreadArgs args[NUM_THREADS];

    int rows_per_thread = g_in.h / NUM_THREADS; // Número de linhas por thread

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].start_row = i * rows_per_thread; // Linha inicial do bloco
        if (i == NUM_THREADS - 1) args[i].end_row = g_in.h; // Última thread pega resto
        else args[i].end_row = (i + 1) * rows_per_thread;  // Linha final (exclusiva)

        // Cria a thread para processar o bloco
        pthread_create(&threads[i], nullptr, process_lines, &args[i]);
    }

    // Espera todas as threads terminarem
    for (int i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], nullptr);

    std::cout << "Worker: processado! Salvando imagem..." << std::endl;

    // Salva a imagem processada
    write_pgm(outpath, &g_out);

    std::cout << "Worker: imagem salva em " << outpath << std::endl;

    // Libera memória
    free(g_in.data);
    free(g_out.data);

    return 0; // Fim do programa
}
