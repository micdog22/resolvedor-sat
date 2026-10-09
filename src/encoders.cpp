#include "encoders.h"

#include <algorithm>
#include <istream>
#include <map>
#include <set>
#include <sstream>

namespace sat {
namespace {

// "No máximo um" pela codificação par a par: para cada par, os dois não podem ser
// verdadeiros ao mesmo tempo. Simples e boa para grupos pequenos.
void at_most_one(Cnf& cnf, const std::vector<int>& vars) {
  for (std::size_t i = 0; i < vars.size(); ++i)
    for (std::size_t j = i + 1; j < vars.size(); ++j) cnf.add({-vars[i], -vars[j]});
}

void exactly_one(Cnf& cnf, const std::vector<int>& vars) {
  cnf.add(vars);
  at_most_one(cnf, vars);
}

bool is_true(const std::vector<LBool>& model, int dimacsVar) {
  const std::size_t v = std::size_t(dimacsVar - 1);
  return v < model.size() && model[v] == LBool::True;
}

}  // namespace

// ---- Sudoku -------------------------------------------------------------

int sudoku_var(int row, int col, int digit) { return 81 * row + 9 * col + digit; }

bool parse_sudoku(const std::string& text, SudokuGrid& grid, std::string& error) {
  int count = 0;
  for (char ch : text) {
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '|' || ch == '-' || ch == '+') continue;
    int digit;
    if (ch >= '1' && ch <= '9')
      digit = ch - '0';
    else if (ch == '0' || ch == '.')
      digit = 0;
    else {
      error = std::string("caractere inesperado no Sudoku: '") + ch + "'";
      return false;
    }
    if (count == 81) {
      error = "o Sudoku tem mais de 81 casas";
      return false;
    }
    grid[std::size_t(count++)] = digit;
  }
  if (count != 81) {
    error = "o Sudoku precisa de 81 casas, encontrei " + std::to_string(count);
    return false;
  }
  // Pistas repetidas tornam o problema impossível; melhor avisar com precisão.
  for (int a = 0; a < 81; ++a) {
    for (int b = a + 1; b < 81; ++b) {
      if (!grid[std::size_t(a)] || grid[std::size_t(a)] != grid[std::size_t(b)]) continue;
      const int ra = a / 9, ca = a % 9, rb = b / 9, cb = b % 9;
      const char* where = ra == rb                                    ? "na mesma linha"
                          : ca == cb                                  ? "na mesma coluna"
                          : (ra / 3 == rb / 3 && ca / 3 == cb / 3) ? "no mesmo bloco"
                                                                      : nullptr;
      if (where) {
        error = "o " + std::to_string(grid[std::size_t(a)]) + " aparece duas vezes " + where + " (linha " +
                std::to_string(ra + 1) + ", coluna " + std::to_string(ca + 1) + " e linha " + std::to_string(rb + 1) +
                ", coluna " + std::to_string(cb + 1) + ")";
        return false;
      }
    }
  }
  return true;
}

Cnf encode_sudoku(const SudokuGrid& puzzle) {
  Cnf cnf;
  cnf.numVars = 729;
  std::vector<int> group;
  for (int r = 0; r < 9; ++r)
    for (int c = 0; c < 9; ++c) {
      group.clear();
      for (int d = 1; d <= 9; ++d) group.push_back(sudoku_var(r, c, d));
      exactly_one(cnf, group);  // cada casa tem exatamente um dígito
    }
  for (int d = 1; d <= 9; ++d) {
    for (int r = 0; r < 9; ++r) {
      group.clear();
      for (int c = 0; c < 9; ++c) group.push_back(sudoku_var(r, c, d));
      exactly_one(cnf, group);  // cada dígito uma vez por linha
    }
    for (int c = 0; c < 9; ++c) {
      group.clear();
      for (int r = 0; r < 9; ++r) group.push_back(sudoku_var(r, c, d));
      exactly_one(cnf, group);  // cada dígito uma vez por coluna
    }
    for (int b = 0; b < 9; ++b) {
      group.clear();
      for (int i = 0; i < 9; ++i) group.push_back(sudoku_var(3 * (b / 3) + i / 3, 3 * (b % 3) + i % 3, d));
      exactly_one(cnf, group);  // cada dígito uma vez por bloco 3x3
    }
  }
  for (int i = 0; i < 81; ++i)
    if (puzzle[std::size_t(i)]) cnf.add({sudoku_var(i / 9, i % 9, puzzle[std::size_t(i)])});  // pistas
  return cnf;
}

SudokuGrid decode_sudoku(const std::vector<LBool>& model) {
  SudokuGrid grid{};
  for (int r = 0; r < 9; ++r)
    for (int c = 0; c < 9; ++c)
      for (int d = 1; d <= 9; ++d)
        if (is_true(model, sudoku_var(r, c, d))) grid[std::size_t(9 * r + c)] = d;
  return grid;
}

bool sudoku_solution_ok(const SudokuGrid& puzzle, const SudokuGrid& solution) {
  for (int i = 0; i < 81; ++i) {
    if (solution[std::size_t(i)] < 1 || solution[std::size_t(i)] > 9) return false;
    if (puzzle[std::size_t(i)] && puzzle[std::size_t(i)] != solution[std::size_t(i)]) return false;
  }
  for (int g = 0; g < 9; ++g) {
    std::set<int> row, col, box;
    for (int i = 0; i < 9; ++i) {
      row.insert(solution[std::size_t(9 * g + i)]);
      col.insert(solution[std::size_t(9 * i + g)]);
      box.insert(solution[std::size_t(9 * (3 * (g / 3) + i / 3) + 3 * (g % 3) + i % 3)]);
    }
    if (row.size() != 9 || col.size() != 9 || box.size() != 9) return false;
  }
  return true;
}

std::string format_sudoku(const SudokuGrid& grid) {
  std::string out;
  for (int r = 0; r < 9; ++r) {
    if (r == 3 || r == 6) out += "------+-------+------\n";
    for (int c = 0; c < 9; ++c) {
      if (c == 3 || c == 6) out += "| ";
      const int d = grid[std::size_t(9 * r + c)];
      out += d ? char('0' + d) : '.';
      out += c == 8 ? "\n" : " ";
    }
  }
  return out;
}

std::vector<int> sudoku_blocking_clause(const SudokuGrid& solution) {
  std::vector<int> clause;
  for (int i = 0; i < 81; ++i) clause.push_back(-sudoku_var(i / 9, i % 9, solution[std::size_t(i)]));
  return clause;
}

// ---- N rainhas ------------------------------------------------------------

int queen_var(int n, int row, int col) { return row * n + col + 1; }

Cnf encode_queens(int n) {
  Cnf cnf;
  cnf.numVars = n * n;
  std::vector<int> group;
  for (int r = 0; r < n; ++r) {
    group.clear();
    for (int c = 0; c < n; ++c) group.push_back(queen_var(n, r, c));
    exactly_one(cnf, group);  // uma rainha por linha
  }
  for (int c = 0; c < n; ++c) {
    group.clear();
    for (int r = 0; r < n; ++r) group.push_back(queen_var(n, r, c));
    exactly_one(cnf, group);  // uma por coluna
  }
  // Diagonais: l - c constante e l + c constante, no máximo uma rainha em cada.
  for (int k = -(n - 1); k <= n - 1; ++k) {
    group.clear();
    for (int r = 0; r < n; ++r)
      if (r - k >= 0 && r - k < n) group.push_back(queen_var(n, r, r - k));
    at_most_one(cnf, group);
  }
  for (int k = 0; k <= 2 * (n - 1); ++k) {
    group.clear();
    for (int r = 0; r < n; ++r)
      if (k - r >= 0 && k - r < n) group.push_back(queen_var(n, r, k - r));
    at_most_one(cnf, group);
  }
  return cnf;
}

std::vector<int> decode_queens(int n, const std::vector<LBool>& model) {
  std::vector<int> columns(std::size_t(n), -1);
  for (int r = 0; r < n; ++r)
    for (int c = 0; c < n; ++c)
      if (is_true(model, queen_var(n, r, c))) columns[std::size_t(r)] = c;
  return columns;
}

bool queens_ok(const std::vector<int>& columns) {
  const int n = int(columns.size());
  for (int r = 0; r < n; ++r) {
    if (columns[std::size_t(r)] < 0 || columns[std::size_t(r)] >= n) return false;
    for (int s = r + 1; s < n; ++s) {
      const int a = columns[std::size_t(r)], b = columns[std::size_t(s)];
      if (a == b || a - b == r - s || a - b == s - r) return false;
    }
  }
  return true;
}

std::string format_queens(const std::vector<int>& columns) {
  std::string out;
  const int n = int(columns.size());
  for (int r = 0; r < n; ++r) {
    for (int c = 0; c < n; ++c) {
      out += columns[std::size_t(r)] == c ? 'D' : '.';
      out += c + 1 == n ? '\n' : ' ';
    }
  }
  return out;
}

std::vector<int> queens_blocking_clause(int n, const std::vector<int>& columns) {
  std::vector<int> clause;
  for (int r = 0; r < n; ++r) clause.push_back(-queen_var(n, r, columns[std::size_t(r)]));
  return clause;
}

// ---- Coloração de grafos ------------------------------------------------------

bool read_graph(std::istream& in, Graph& graph, std::string& error) {
  graph = Graph();
  std::map<std::string, int> ids;
  std::set<std::pair<int, int>> seen;
  auto id_of = [&](const std::string& name) {
    auto it = ids.find(name);
    if (it != ids.end()) return it->second;
    const int id = int(graph.names.size());
    ids[name] = id;
    graph.names.push_back(name);
    return id;
  };
  std::string line;
  int lineNumber = 0;
  while (std::getline(in, line)) {
    ++lineNumber;
    const std::size_t hash = line.find('#');
    if (hash != std::string::npos) line.resize(hash);
    std::istringstream is(line);
    std::vector<std::string> tokens;
    for (std::string t; is >> t;) tokens.push_back(t);
    if (tokens.empty()) continue;
    if (tokens.size() > 2) {
      error = "linha " + std::to_string(lineNumber) + ": esperava 'u v' (uma aresta por linha)";
      return false;
    }
    const int u = id_of(tokens[0]);
    if (tokens.size() == 1) continue;  // vértice isolado
    const int v = id_of(tokens[1]);
    if (u == v) {
      error = "linha " + std::to_string(lineNumber) + ": laço em '" + tokens[0] +
              "' (um vértice vizinho dele mesmo nunca pode ser colorido)";
      return false;
    }
    if (seen.insert({std::min(u, v), std::max(u, v)}).second) graph.edges.push_back({u, v});
  }
  if (graph.names.empty()) {
    error = "o grafo está vazio";
    return false;
  }
  return true;
}

Cnf encode_coloring(const Graph& graph, int colors) {
  Cnf cnf;
  const int n = int(graph.names.size());
  cnf.numVars = n * colors;
  auto x = [&](int v, int c) { return v * colors + c + 1; };
  std::vector<int> group;
  for (int v = 0; v < n; ++v) {
    group.clear();
    for (int c = 0; c < colors; ++c) group.push_back(x(v, c));
    exactly_one(cnf, group);  // cada vértice tem exatamente uma cor
  }
  for (const auto& e : graph.edges)
    for (int c = 0; c < colors; ++c) cnf.add({-x(e.first, c), -x(e.second, c)});  // vizinhos com cores diferentes
  // Quebra de simetria: trocar os nomes das cores não muda nada, então o primeiro
  // vértice pode ficar sempre com a primeira cor.
  if (n > 0 && colors > 0) cnf.add({x(0, 0)});
  return cnf;
}

std::vector<int> decode_coloring(const Graph& graph, int colors, const std::vector<LBool>& model) {
  const int n = int(graph.names.size());
  std::vector<int> color(std::size_t(n), -1);
  for (int v = 0; v < n; ++v)
    for (int c = 0; c < colors; ++c)
      if (is_true(model, v * colors + c + 1)) color[std::size_t(v)] = c;
  return color;
}

bool coloring_ok(const Graph& graph, int colors, const std::vector<int>& color) {
  if (color.size() != graph.names.size()) return false;
  for (int c : color)
    if (c < 0 || c >= colors) return false;
  for (const auto& e : graph.edges)
    if (color[std::size_t(e.first)] == color[std::size_t(e.second)]) return false;
  return true;
}

std::string color_name(int color) {
  static const char* const names[] = {"vermelho", "azul", "verde", "amarelo", "roxo", "laranja", "rosa", "cinza"};
  if (color >= 0 && color < 8) return names[color];
  return "cor " + std::to_string(color + 1);
}

// ---- Casa dos pombos ------------------------------------------------------

Cnf encode_pigeonhole(int holes) {
  Cnf cnf;
  const int pigeons = holes + 1;
  cnf.numVars = pigeons * holes;
  auto p = [&](int pigeon, int hole) { return pigeon * holes + hole + 1; };
  for (int i = 0; i < pigeons; ++i) {
    std::vector<int> somewhere;
    for (int j = 0; j < holes; ++j) somewhere.push_back(p(i, j));
    cnf.add(somewhere);  // todo pombo está em alguma casa
  }
  for (int j = 0; j < holes; ++j)
    for (int a = 0; a < pigeons; ++a)
      for (int b = a + 1; b < pigeons; ++b) cnf.add({-p(a, j), -p(b, j)});  // dois pombos não dividem casa
  return cnf;
}

}  // namespace sat
