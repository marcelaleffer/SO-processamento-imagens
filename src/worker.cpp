#include <iostream>     // cout/cerr
#include <pthread.h>    // threads, mutex
#include <semaphore.h>  // semáforos POSIX (sem_init, sem_wait, sem_post)
#include <fcntl.h>      // open()
#include <sys/stat.h>   // mkfifo()
#include <unistd.h>     // read(), close()
#include <cstring>      // strcmp()
#include <vector>
#include "pgm_utils.h"
#include "filters.h"

using namespace std;

/*=========================
  Configuração geral
=========================*/
static const char* FIFO_PATH = "/tmp/imgpipe";
static const int   NUM_THREADS_DEFAULT = 4;
static const int   ROWS_PER_TASK = 64; // granulação das tarefas

// Imagens globais compartilhadas (somente leitura em g_in, escrita em g_out)
PGM g_in, g_out;

// Modo do filtro: 0 = NEGATIVO, 1 = SLICE
int g_mode = 0;
int g_t1 = 0, g_t2 = 0;

// Número de threads do pool
int g_nthreads = NUM_THREADS_DEFAULT;

/*=========================
  Tarefas e Fila (bounded)
=========================*/
struct Task {
    int row_start;  // inclusivo
    int row_end;    // exclusivo
};

// Sentinela que indica fim
static const Task SENTINEL = {-1, -1};

#define QMAX 128
Task qbuf[QMAX];
int   q_head = 0, q_tail = 0;

// Sincronização da fila
pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;
sem_t sem_items;  // quantas tarefas disponíveis
sem_t sem_space;  // espaços livres no buffer

// Enfileirar tarefa (produtor)
void q_push(Task t) {
    sem_wait(&sem_space);               // espera espaço
    pthread_mutex_lock(&q_lock);        // exclusão mútua
    qbuf[q_tail] = t;
    q_tail = (q_tail + 1) % QMAX;
    pthread_mutex_unlock(&q_lock);
    sem_post(&sem_items);               // sinaliza item disponível
}

// Retirar tarefa (consumidor)
Task q_pop() {
    sem_wait(&sem_items);               // espera item
    pthread_mutex_lock(&q_lock);
    Task t = qbuf[q_head];
    q_head = (q_head + 1) % QMAX;
    pthread_mutex_unlock(&q_lock);
    sem_post(&sem_space);               // libera espaço
    return t;
}

/*=========================
  Thread trabalhadora
=========================*/
void* worker_thread(void*) {
    while (true) {
        Task t = q_pop();
        if (t.row_start < 0 && t.row_end < 0) {
            // Sentinela: termina a thread
            break;
        }
        if (g_mode == 0) {
            apply_negative(&g_in, &g_out, t.row_start, t.row_end);
        } else {
            apply_slice(&g_in, &g_out, t.row_start, t.row_end, g_t1, g_t2);
        }
    }
    return nullptr;
}

/*=========================
  Main do processo worker
=========================*/
int main(int argc, char** argv) {
    // Uso: ./worker <saida.pgm> <negativo|slice> [t1 t2] [nthreads]
    if (argc < 3) {
        cerr << "Uso: " << argv[0] << " <saida.pgm> <negativo|slice> [t1 t2] [nthreads]\n";
        return -1;
    }

    const char* outpath = argv[1];
    const char* mode    = argv[2];

    if (strcmp(mode, "negativo") == 0) {
        g_mode = 0;
        if (argc >= 4) g_nthreads = atoi(argv[3]); // opcional: permitir ./worker out.pgm negativo 8
    } else if (strcmp(mode, "slice") == 0) {
        if (argc < 5) {
            cerr << "Para slice, use: " << argv[0] << " <saida.pgm> slice <t1> <t2> [nthreads]\n";
            return -1;
        }
        g_mode = 1;
        g_t1 = atoi(argv[3]);
        g_t2 = atoi(argv[4]);
        if (argc >= 6) g_nthreads = atoi(argv[5]);
    } else {
        cerr << "Modo invalido! Use 'negativo' ou 'slice'.\n";
        return -1;
    }

    // Cria FIFO (ignora erro se já existir)
    mkfifo(FIFO_PATH, 0666);

    // Abre FIFO para leitura (bloqueia até o sender abrir escrita)
    int fifo = open(FIFO_PATH, O_RDONLY);
    if (fifo == -1) {
        perror("Erro ao abrir FIFO");
        return -1;
    }

    // Lê metadados
    if (read(fifo, &g_in.w, sizeof(g_in.w)) != sizeof(g_in.w) ||
        read(fifo, &g_in.h, sizeof(g_in.h)) != sizeof(g_in.h) ||
        read(fifo, &g_in.maxv, sizeof(g_in.maxv)) != sizeof(g_in.maxv)) {
        perror("Erro ao ler metadados");
        close(fifo);
        return -1;
    }

    // Prepara cabeçalho de saída
    g_out.w = g_in.w;
    g_out.h = g_in.h;
    g_out.maxv = g_in.maxv;

    cout << "Worker: metadados recebidos -> w=" << g_in.w
         << " h=" << g_in.h << " maxv=" << g_in.maxv << endl;

    // Aloca buffers
    size_t total_bytes = static_cast<size_t>(g_in.w) * g_in.h;
    g_in.data  = (unsigned char*)malloc(total_bytes);
    g_out.data = (unsigned char*)malloc(total_bytes);
    if (!g_in.data || !g_out.data) {
        cerr << "Falha ao alocar memoria.\n";
        close(fifo);
        return -1;
    }

    // Lê todos os pixels (loop robusto)
    size_t bytes_read = 0;
    while (bytes_read < total_bytes) {
        ssize_t r = read(fifo, g_in.data + bytes_read, total_bytes - bytes_read);
        if (r <= 0) {
            perror("Erro ao ler pixels do FIFO");
            free(g_in.data);
            free(g_out.data);
            close(fifo);
            return -1;
        }
        bytes_read += (size_t)r;
    }
    close(fifo);

    cout << "Worker: recebimento concluido. Processando com " << g_nthreads << " threads...\n";

    // --- INÍCIO medição de tempo ---
    timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    // ------------------------------

    // Inicializa semáforos da fila
    sem_init(&sem_items, 0, 0);        // começa sem itens
    sem_init(&sem_space, 0, QMAX);     // buffer começa cheio de espaço

    // Cria pool de threads
    vector<pthread_t> threads(g_nthreads);
    for (int i = 0; i < g_nthreads; ++i) {
        pthread_create(&threads[i], nullptr, worker_thread, nullptr);
    }

    // PRODUZIR tarefas em blocos (particionamento dinâmico)
    for (int rs = 0; rs < g_in.h; rs += ROWS_PER_TASK) {
        int re = rs + ROWS_PER_TASK;
        if (re > g_in.h) re = g_in.h;
        q_push({rs, re});
    }

    // Enfileira N sentinelas para encerrar as threads
    for (int i = 0; i < g_nthreads; ++i) q_push(SENTINEL);

    // Espera threads
    for (int i = 0; i < g_nthreads; ++i) {
        pthread_join(threads[i], nullptr);
    }

    // --- FIM da medição ---
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                    (end.tv_nsec - start.tv_nsec) / 1e9;
    cout << "Tempo de processamento paralelo: " << elapsed << " segundos\n";
    // ----------------------

    cout << "Worker: processamento concluido. Salvando...\n";
    // Grava result
    if (write_pgm(outpath, &g_out) != 0) {
        cerr << "Erro ao salvar " << outpath << endl;
    } else {
        cout << "Worker: imagem salva em " << outpath << endl;
    }

    // Limpa recursos
    free(g_in.data);
    free(g_out.data);
    sem_destroy(&sem_items);
    sem_destroy(&sem_space);
    // q_lock é estático — não precisa destruir para este programa simples

    return 0;
}
