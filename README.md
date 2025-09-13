# SO - Processamento de Imagens

Projeto em C++ que aplica filtros em imagens PGM usando **FIFO** e **threads**.
--

##Comandos - Gerar imagem com filtro negativo:

### Terminal 1:
- rm /tmp/imgpipe
- mkfifo /tmp/imgpipe
- g++ src/sender.cpp src/pgm_utils.cpp -o sender -lpthread
- g++ src/worker.cpp src/pgm_utils.cpp src/filters.cpp -o worker -lpthread
- ./worker out/resultado-negativo.pgm negativo

### Termminal 2:
- ./sender img/templo_do_sol.pgm

--

##Comandos - Gerar imagem com filtro de limiarização com fatiamento:

### Terminal 1:
- rm /tmp/imgpipe
- mkfifo /tmp/imgpipe
- g++ src/sender.cpp src/pgm_utils.cpp -o sender -lpthread
- g++ src/worker.cpp src/pgm_utils.cpp src/filters.cpp -o worker -lpthread
- ./worker out/resultado-limiarizacao.pgm slice 80 160

### Termminal 2:
- ./sender img/templo_do_sol.pgm

*Observações:*
- Inicie sempre o worker antes do sender.
- No modo slice, pixels fora de [t1, t2] viram branco (255); dentro do intervalo, mantêm o tom original.