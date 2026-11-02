Os testes são simples, apenas para explicitar o funcionamento do mecanismo.
É necessário seguir alguns passos para a reprodução.

1. Compilar o programa factorials, basta ir até o diretório test_programs
e executar make factorials.
2. Compilar o gerador de traço, basta ir até o diretório x86_trace_generator
e executar make
3. Gerar o traço, basta se manter no diretório do gerador de traços e executar
./pin/pin -t ./obj-intel64/sinuca3_pintool.so -o ../traces -- ../test_programs/factorials
4. Compilar o simulador, basta retornar ao diretório do SiNUCA3 e compilar com 
make debug.
5. Executar a simulação, basta executar no diretório do SiNUCA3
./sinuca3-debug -c config_files/engine_debug.yaml -t ./traces/factorials
6. Para inverter a ordem dos componentes, é necessário ir até o arquivo engine.cpp, na linha
286 e inverter a lógica do for, transformando em:
for (long i = this->numberOfComponents - 1; i >= 0; --i)
Então, apenas compile e execute novamente o simulador.