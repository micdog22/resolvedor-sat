# Resolvedor SAT

Um resolvedor SAT do tipo CDCL escrito do zero em C++17, sem dependências, no estilo do MiniSat e do Glucose: dois literais vigiados, VSIDS, aprendizado pelo primeiro UIP com minimização recursiva, retrocesso não cronológico, reinícios, salvamento de fase e limpeza de cláusulas pelo LBD. Vem com codificadores prontos para Sudoku, N rainhas, coloração de grafos (inclusive o mapa do Brasil) e a casa dos pombos, e a seção [Como funciona](#como-funciona) explica cada técnica com exemplos.

## Recursos

- **Leitor DIMACS CNF** robusto: comentários, cláusulas que atravessam linhas, várias cláusulas por linha, o marcador `%` dos arquivos da SATLIB e mensagens de erro em português com o número da linha. Aceita até 10 milhões de variáveis e não confia no cabeçalho para reservar memória.
- **CDCL completo**:
  - propagação unitária com **dois literais vigiados** (e literal bloqueador);
  - heurística **VSIDS** com decaimento, num heap binário;
  - análise de conflitos pelo **primeiro UIP** com **minimização recursiva** da cláusula aprendida;
  - **retrocesso não cronológico** (backjumping);
  - **reinícios** no estilo **Glucose** (pela média recente do LBD, com bloqueio) ou pela sequência de **Luby**;
  - **salvamento de fase**;
  - **limpeza das cláusulas aprendidas pelo LBD**, preservando as cláusulas "cola".
- **Saída no formato das competições SAT**: `s SATISFIABLE` com as linhas `v ... 0`, `s UNSATISFIABLE` ou `s UNKNOWN`, e códigos de saída 10, 20 e 0. O modelo é conferido contra todas as cláusulas antes de ser impresso.
- **Estatísticas** (`--estatisticas`): conflitos, decisões, propagações, reinícios, cláusulas aprendidas e apagadas, quanto a minimização cortou e o tempo.
- **Codificadores**: `sudoku` (grade 9x9, com verificação de solução única), `rainhas N` (uma, todas ou só a contagem), `coloracao` (lista de arestas) e `pombos N` (gerador da casa dos pombos). Todos podem imprimir a fórmula em DIMACS com `--cnf`, para usar em outros resolvedores.
- Uso **incremental**: dá para acrescentar cláusulas depois de uma resposta e resolver de novo, o que é usado para contar soluções com cláusulas de bloqueio.

## Como usar

Depois de compilar (veja [Como compilar](#como-compilar)), o executável fica em `build/sat`.

```
sat [opções] <arquivo.cnf | ->        resolve uma fórmula DIMACS (- lê da entrada padrão)
sat sudoku [arquivo | -] [opções]     resolve um Sudoku 9x9 (0 ou . para casa vazia)
sat rainhas <N> [--todas | --contar]  coloca N rainhas sem ataques entre elas
sat coloracao <arquivo> --cores <K>   colore um grafo dado por lista de arestas
sat pombos <N>                        gera a fórmula da casa dos pombos (N+1 pombos, N casas)
```

| Opção | Efeito |
|---|---|
| `--estatisticas` | estatísticas da busca em linhas começando com `c` |
| `--reinicio glucose\|luby` | política de reinícios (padrão: glucose) |
| `--sem-minimizacao` | desliga a minimização recursiva |
| `--limite <conflitos>` | desiste depois de tantos conflitos e responde `s UNKNOWN` |
| `--sem-modelo` | não imprime as linhas `v` |
| `--cnf` | nos codificadores, só imprime a fórmula em DIMACS |

Códigos de saída: 10 satisfatível, 20 insatisfatível, 0 indefinido (limite atingido), 1 erro na entrada e 2 uso incorreto.

### Uma fórmula DIMACS

```console
$ cat exemplos/formula.cnf
c Exemplo pequeno: (x1 ou x2) e (não x1 ou x3) e (não x2 ou não x3) e (x2 ou x3).
c Tem dois modelos: (x1, x2, x3) = (falso, verdadeiro, falso) ou (verdadeiro, falso, verdadeiro).
p cnf 3 4
1 2 0
-1 3 0
-2 -3 0
2 3 0
$ ./build/sat exemplos/formula.cnf
s SATISFIABLE
v -1 2 -3 0
```

### Casa dos pombos, com estatísticas

```console
$ ./build/sat pombos 9 > pombos9.cnf
$ ./build/sat --estatisticas pombos9.cnf
c Resolvedor SAT 1.0
c variáveis: 90  cláusulas originais: 176
c conflitos: 101427  decisões: 122794  propagações: 1277363
c reinícios: 321 (bloqueados: 158)
c cláusulas aprendidas: 101426 (unitárias: 6)  apagadas: 101455  limpezas: 20
c tamanho médio das aprendidas: 27,2 literais (a minimização removeu 14,4%)
c tempo: 1,679 s
s UNSATISFIABLE
```

### Sudoku

```console
$ cat exemplos/sudoku-facil.txt
53..7....
6..195...
.98....6.
8...6...3
4..8.3..1
7...2...6
.6....28.
...419..5
....8..79
$ ./build/sat sudoku exemplos/sudoku-facil.txt
Solução (única):
5 3 4 | 6 7 8 | 9 1 2
6 7 2 | 1 9 5 | 3 4 8
1 9 8 | 3 4 2 | 5 6 7
------+-------+------
8 5 9 | 7 6 1 | 4 2 3
4 2 6 | 8 5 3 | 7 9 1
7 1 3 | 9 2 4 | 8 5 6
------+-------+------
9 6 1 | 5 3 7 | 2 8 4
2 8 7 | 4 1 9 | 6 3 5
3 4 5 | 2 8 6 | 1 7 9
```

O arquivo `exemplos/sudoku-dificil.txt` traz o Sudoku que Arto Inkala divulgou em 2012 como "o mais difícil do mundo"; para o resolvedor ele leva milissegundos. Depois de achar uma solução o programa a proíbe e resolve de novo: se não aparecer outra, ela é única.

### N rainhas

```console
$ ./build/sat rainhas 8 --contar
Total: 92 soluções para 8 rainhas.
$ ./build/sat rainhas 6 --todas
...
Solução 4:
. . D . . .
. . . . . D
. D . . . .
. . . . D .
D . . . . .
. . . D . .

Total: 4 soluções para 6 rainhas.
```

### Coloração: o mapa do Brasil

O arquivo `exemplos/brasil.txt` tem as 51 fronteiras entre as 27 unidades da federação, uma aresta por linha (`GO MG`):

```console
$ ./build/sat coloracao exemplos/brasil.txt --cores 3
O grafo (27 vértices, 51 arestas) não pode ser colorido com 3 cores.
$ ./build/sat coloracao exemplos/brasil.txt --cores 4
Coloração com 4 cores (27 vértices, 51 arestas):
  AC: vermelho
  AL: verde
  AM: verde
  ...
  GO: azul
  ...
  TO: verde
```

Por que 3 cores não bastam? Goiás faz fronteira com Tocantins, Bahia, Minas Gerais, Mato Grosso do Sul e Mato Grosso, e esses cinco formam um ciclo (TO, BA, MG, MS, MT, de volta a TO). Um ciclo ímpar precisa de 3 cores sozinho, e Goiás, vizinho de todos eles, precisa de uma quarta.

## Como compilar

Precisa só de um compilador C++17 (clang ou gcc).

```bash
make            # gera build/sat
make test       # testes de unidade e de linha de comando
make clean
```

Ou com CMake:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Testado no macOS com clang. O código usa só a biblioteca padrão; os testes de linha de comando usam `sh`.

## Testes

```bash
make test    # testes de unidade + testes da linha de comando
make asan    # os mesmos testes com AddressSanitizer e UndefinedBehaviorSanitizer
```

- **Leitor DIMACS**: arquivos válidos (comentários, cláusula em várias linhas, `%` da SATLIB, CRLF) e 15 tipos de erro, com a linha certa na mensagem.
- **Fórmulas feitas à mão**: fórmula vazia, cláusula vazia, contradição direta, as 8 cláusulas sobre 3 variáveis (insatisfatível) e as 7 que deixam um único modelo, cadeia de 60 implicações, literais repetidos e tautologias.
- **Casa dos pombos** PHP(n+1, n) insatisfatível para n de 1 a 8, e a versão com n pombos e n casas satisfatível como controle.
- **3-SAT aleatório na razão 4,26** (onde metade das fórmulas é satisfatível): para n de 5 a 20 a resposta é conferida por força bruta em todas as 2^n atribuições; para n de 50 a 150 todo modelo é conferido contra todas as cláusulas e a resposta é comparada com outra configuração (reinícios de Luby e sem minimização).
- **Uso incremental**: o número de modelos de fórmulas pequenas, contado com cláusulas de bloqueio, bate com a força bruta.
- **Sudoku** fácil e difícil: solução válida, igual à conhecida e única; entradas inválidas e um Sudoku sem solução.
- **N rainhas**: de 1 a 8 rainhas, as contagens 1, 0, 0, 2, 10, 4, 40 e 92, todas as soluções válidas e diferentes.
- **Coloração**: grafo de Petersen (não sai com 2 cores, sai com 3), mapa do Brasil (não sai com 3, sai com 4), triângulo e erros de entrada.
- **Linha de comando**: saída `s`/`v`, códigos 10, 20, 0, 1 e 2, entrada padrão, limite de conflitos e todos os subcomandos.

## Como funciona

### O problema SAT e a forma normal conjuntiva

Dada uma fórmula booleana, existe uma atribuição de verdadeiro e falso às variáveis que a torna verdadeira? Esse é o problema SAT, o primeiro problema provado NP-completo (Cook, 1971). Na prática, resolvedores modernos decidem fórmulas industriais com milhões de variáveis, e muito problema difícil (escalonamento, verificação de hardware, planejamento, quebra-cabeças) é resolvido traduzindo para SAT.

Os resolvedores trabalham com a **forma normal conjuntiva** (CNF): um E de cláusulas, cada cláusula um OU de literais, e cada literal uma variável ou a sua negação.

```
(x1 ∨ x2) ∧ (¬x1 ∨ x3) ∧ (¬x2 ∨ ¬x3) ∧ (x2 ∨ x3)
```

No formato **DIMACS** as variáveis são números a partir de 1, a negação é o sinal de menos e cada cláusula termina em 0:

```
p cnf 3 4      <- 3 variáveis, 4 cláusulas
1 2 0
-1 3 0
-2 -3 0
2 3 0
```

### DPLL: busca com propagação

O algoritmo clássico (Davis, Putnam, Logemann e Loveland, 1962) é uma busca em profundidade: escolhe uma variável, tenta um valor e, se der errado, tenta o outro. O que o torna prático é a **propagação unitária**: se todos os literais de uma cláusula são falsos menos um, esse último é obrigatório. Cada decisão costuma forçar uma cascata de atribuições.

O ponto fraco do DPLL é que, ao achar um conflito, ele volta só uma decisão (retrocesso cronológico) e esquece o motivo do conflito. O mesmo erro pode ser redescoberto milhares de vezes em outros ramos.

### CDCL: aprender com os conflitos

O CDCL (*conflict-driven clause learning*) muda duas coisas:

1. A cada conflito, analisa **por que** ele aconteceu e aprende uma cláusula nova que impede a mesma combinação de se repetir.
2. Volta direto ao nível onde essa cláusula aprendida passa a forçar alguma coisa, pulando decisões que não tiveram nada a ver com o conflito (**retrocesso não cronológico**).

O laço principal fica assim:

```
enquanto verdadeiro:
    conflito = propaga()
    se houve conflito:
        se estamos no nível 0: insatisfatível
        cláusula, nível = analisa(conflito)
        volta até o nível; acrescenta a cláusula (ela força um literal)
    senão:
        se é hora de reiniciar: volta ao nível 0
        se todas as variáveis têm valor: satisfatível
        decide uma variável (VSIDS) com a fase salva
```

### Exemplo: grafo de implicação e primeiro UIP

Considere estas cláusulas:

```
c1: (¬x3 ∨ x4)          x3 implica x4
c2: (¬x1 ∨ ¬x4 ∨ x5)    x1 e x4 implicam x5
c3: (¬x4 ∨ x6)          x4 implica x6
c4: (¬x5 ∨ ¬x6)         x5 e x6 não podem ser verdadeiros juntos
```

O resolvedor decide `x1 = 1` no nível 1, `x2 = 1` no nível 2 (x2 não aparece nessas cláusulas) e `x3 = 1` no nível 3. A propagação monta o **grafo de implicação**, em que cada atribuição forçada aponta para as que a causaram:

```
nível 1   x1=1 ------------------+
                                 | c2
nível 3   x3=1 --c1--> x4=1 -----+--> x5=1 --+
                         |                    +--c4--> CONFLITO
                         +--c3--> x6=1 -------+
```

A análise começa pela cláusula em conflito e vai trocando cada literal do nível atual pela sua razão (isso é uma **resolução**), andando para trás na trilha:

```
c4:              (¬x5 ∨ ¬x6)        dois literais do nível 3
resolve com c3:  (¬x5 ∨ ¬x4)        x6 foi forçado por c3
resolve com c2:  (¬x4 ∨ ¬x1)        x5 foi forçado por c2; só x4 é do nível 3: pare
```

Quando sobra exatamente um literal do nível atual, ele é o **primeiro UIP** (*unique implication point*): um nó por onde passam todos os caminhos da decisão até o conflito. Aqui o UIP é `x4`, e não a decisão `x3`. A cláusula aprendida é `(¬x4 ∨ ¬x1)`: "x1 e x4 juntos levam a conflito".

O outro literal dela é do nível 1, então o resolvedor **volta direto ao nível 1**, pulando o nível 2 inteiro (a decisão sobre x2 era irrelevante). Lá a cláusula aprendida é unitária e força `x4 = 0`, e c1 em seguida força `x3 = 0`. Uma busca cronológica teria testado `x2 = 0` antes, à toa.

### Minimização recursiva

Muitas vezes um literal da cláusula aprendida é consequência dos outros. Se a razão de `¬a` só contém literais que já estão na cláusula (ou que, recursivamente, também são implicados por eles), `¬a` é **redundante** e pode sair. Cláusulas menores propagam mais e ocupam menos memória. Nos testes com a casa dos pombos a minimização corta de 10% a 20% dos literais. Um filtro barato evita buscas inúteis: só vale a pena descer por literais cujos níveis de decisão aparecem na cláusula (testado com uma máscara de 32 bits dos níveis).

### Dois literais vigiados

A propagação consome a maior parte do tempo, e a pergunta que ela faz o tempo todo é "esta cláusula ficou unitária?". Em vez de contar literais falsos, cada cláusula **vigia dois literais não falsos**. Enquanto os dois vigiados não forem falsos, a cláusula não pode ser unitária nem conflitante, então nem é visitada. Quando um vigiado fica falso, a cláusula procura outro literal não falso para vigiar; se não houver, ela é unitária (o outro vigiado é forçado) ou conflitante.

```
(a ∨ b ∨ c ∨ d)   vigia a e b
b fica falso  ->  procura substituto: c serve, passa a vigiar a e c
c fica falso  ->  d serve, vigia a e d
d fica falso  ->  não há substituto: a é forçado (ou conflito, se a já for falso)
```

A grande vantagem: ao **retroceder** nada precisa ser atualizado, porque desfazer atribuições nunca torna falso um literal vigiado. Cada vigia também guarda um **literal bloqueador** da cláusula; se ele já é verdadeiro, a cláusula está satisfeita e nem precisa ser lida da memória.

### VSIDS: qual variável decidir

Cada variável tem uma **atividade**. Toda variável que participa de um conflito ganha um incremento, e o incremento cresce cerca de 5% a cada conflito (equivale a multiplicar todas as atividades antigas por 0,95: as recentes pesam mais). A próxima decisão é sempre a variável livre de maior atividade, que fica num heap binário. O efeito é concentrar a busca na parte "quente" do problema, onde os conflitos estão acontecendo.

O **salvamento de fase** completa a heurística: ao decidir uma variável, o resolvedor usa o último valor que ela teve. Depois de um reinício ou de um retrocesso longo, ele reconstrói rapidamente as partes da atribuição que já estavam consistentes.

### Reinícios

Reiniciar (voltar ao nível 0 mantendo as cláusulas aprendidas e as atividades) parece desperdício, mas evita que o resolvedor fique preso numa região ruim por causa das primeiras decisões. Duas políticas:

- **Luby**: reinicia depois de 100 × (1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8, ...) conflitos. Essa sequência é ótima, a menos de um fator constante, quando não se sabe nada sobre o problema.
- **Glucose** (padrão): reinicia quando a média do LBD das últimas 50 cláusulas aprendidas fica pior que 80% da média geral, ou seja, quando o resolvedor está aprendendo cláusulas ruins. Se a trilha estiver bem maior que o normal (talvez perto de uma solução), o reinício é **bloqueado**.

Na casa dos pombos com 9 casas, o Glucose precisou de cerca de 101 mil conflitos e o Luby de 183 mil.

### LBD e limpeza das aprendidas

Guardar todas as cláusulas aprendidas deixaria a propagação cada vez mais lenta. O **LBD** (*literal block distance*) de uma cláusula é quantos níveis de decisão diferentes aparecem nela. Cláusulas com LBD baixo ligam poucos "blocos" de decisões e costumam ser muito úteis; as de LBD 2 são chamadas de cláusulas **cola** (*glue clauses*) e nunca são apagadas.

A cada 2000 conflitos (intervalo que cresce 300 a cada vez) as aprendidas são ordenadas por LBD e atividade, e a metade pior é apagada, menos as binárias, as cola e as que são a razão de alguma atribuição atual. As cláusulas satisfeitas no nível 0 também são removidas.

### Como codificar problemas em SAT

A receita é sempre a mesma: escolher variáveis booleanas que descrevam uma solução e escrever as regras como cláusulas.

**Sudoku**: a variável `x(l, c, d)` diz que a casa da linha `l` e coluna `c` tem o dígito `d`, num total de 9 × 9 × 9 = 729 variáveis. As regras viram restrições "exatamente um":

- cada casa tem exatamente um dígito;
- cada dígito aparece exatamente uma vez em cada linha, cada coluna e cada bloco 3x3;
- cada pista vira uma cláusula unitária.

"Pelo menos um" é uma cláusula só (`x1 ∨ x2 ∨ ... ∨ x9`); "no máximo um" é a codificação par a par (`¬xi ∨ ¬xj` para cada par). São 11.988 cláusulas mais as pistas, e a propagação unitária sozinha já resolve a maioria dos Sudokus fáceis.

**N rainhas**: `q(l, c)` diz que há rainha na casa. Exatamente uma por linha e por coluna, e no máximo uma em cada diagonal. Para contar todas as soluções, cada solução encontrada vira uma **cláusula de bloqueio** (a negação dela) e o resolvedor continua de onde parou, mantendo tudo o que aprendeu.

**Coloração**: `x(v, k)` diz que o vértice `v` tem a cor `k`. Cada vértice tem exatamente uma cor e, para cada aresta `(u, v)` e cada cor `k`, vale `¬x(u, k) ∨ ¬x(v, k)`. Como trocar os nomes das cores não muda nada, o primeiro vértice é fixado na primeira cor (**quebra de simetria**), o que evita explorar K versões equivalentes de cada tentativa.

**Casa dos pombos**: `p(i, j)` diz que o pombo `i` está na casa `j`. Todo pombo está em alguma casa e dois pombos nunca dividem casa. Com N + 1 pombos e N casas é óbvio que não dá, mas provar isso por resolução exige um número **exponencial** de passos (Haken, 1985). Por isso é um teste de estresse clássico: o tempo cresce rápido, de 0,01 s com 7 casas para 0,14 s com 8 e 1,7 s com 9.

### Estrutura do código

| Arquivo | Conteúdo |
|---|---|
| `src/solver.*` | o resolvedor CDCL |
| `src/cnf.*` | leitura e escrita de DIMACS, conferência de modelos e linhas `v` |
| `src/encoders.*` | Sudoku, N rainhas, coloração e casa dos pombos |
| `src/main.cpp` | linha de comando |
| `exemplos/` | fórmula de exemplo, dois Sudokus, grafo de Petersen e o mapa do Brasil |

## Contribuindo

Issues e pull requests são bem-vindos.

## Licença

MIT. Veja [LICENSE](LICENSE).
