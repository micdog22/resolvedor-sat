// Codificadores: transformam problemas em CNF e traduzem o modelo de volta.
// Sudoku 9x9, N rainhas, coloração de grafos e a casa dos pombos.
#ifndef SAT_ENCODERS_H
#define SAT_ENCODERS_H

#include <array>
#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "cnf.h"

namespace sat {

// ---- Sudoku -------------------------------------------------------------
// x(l, c, d) é verdadeira quando a casa da linha l e coluna c recebe o dígito d.
using SudokuGrid = std::array<int, 81>;  // 0 = casa vazia

// Aceita dígitos de 1 a 9, 0 ou . para casa vazia e ignora espaços, quebras de linha
// e os separadores | - + de grades desenhadas.
bool parse_sudoku(const std::string& text, SudokuGrid& grid, std::string& error);
int sudoku_var(int row, int col, int digit);
Cnf encode_sudoku(const SudokuGrid& puzzle);
SudokuGrid decode_sudoku(const std::vector<LBool>& model);
// Linhas, colunas e blocos com 1 a 9, e as pistas preservadas.
bool sudoku_solution_ok(const SudokuGrid& puzzle, const SudokuGrid& solution);
std::string format_sudoku(const SudokuGrid& grid);
// Cláusula que proíbe exatamente esta solução (para conferir se ela é única).
std::vector<int> sudoku_blocking_clause(const SudokuGrid& solution);

// ---- N rainhas ------------------------------------------------------------
// q(l, c) é verdadeira quando há uma rainha na linha l, coluna c.
int queen_var(int n, int row, int col);
Cnf encode_queens(int n);
std::vector<int> decode_queens(int n, const std::vector<LBool>& model);  // coluna de cada linha
bool queens_ok(const std::vector<int>& columns);
std::string format_queens(const std::vector<int>& columns);
std::vector<int> queens_blocking_clause(int n, const std::vector<int>& columns);

// ---- Coloração de grafos ------------------------------------------------------
struct Graph {
  std::vector<std::string> names;
  std::vector<std::pair<int, int>> edges;
};

// Uma aresta por linha ("u v"); uma linha com um nome só declara um vértice isolado;
// # começa comentário.
bool read_graph(std::istream& in, Graph& graph, std::string& error);
Cnf encode_coloring(const Graph& graph, int colors);
std::vector<int> decode_coloring(const Graph& graph, int colors, const std::vector<LBool>& model);
bool coloring_ok(const Graph& graph, int colors, const std::vector<int>& color);
std::string color_name(int color);

// ---- Casa dos pombos ------------------------------------------------------
// holes + 1 pombos em holes casas, no máximo um pombo por casa: sempre insatisfatível,
// e famosa por exigir provas exponenciais de qualquer resolvedor baseado em resolução.
Cnf encode_pigeonhole(int holes);

}  // namespace sat

#endif
