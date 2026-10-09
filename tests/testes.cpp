// Testes do resolvedor: leitor DIMACS, fórmulas feitas à mão, casa dos pombos,
// 3-SAT aleatório conferido por força bruta, Sudoku, N rainhas e coloração.
#include <cstdint>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "encoders.h"
#include "teste.h"

using namespace sat;

namespace {

std::string examples_dir = "exemplos";

bool contains(const std::string& text, const std::string& piece) { return text.find(piece) != std::string::npos; }

Cnf parse_ok(const std::string& text) {
  std::istringstream in(text);
  Cnf cnf;
  std::string error;
  if (!read_dimacs(in, cnf, error)) std::cerr << "DIMACS inválido no teste: " << error << "\n";
  return cnf;
}

std::string parse_error(const std::string& text) {
  std::istringstream in(text);
  Cnf cnf;
  std::string error;
  if (read_dimacs(in, cnf, error)) return "(sem erro)";
  return error;
}

Result solve_cnf(const Cnf& cnf, std::vector<LBool>* model = nullptr, Options options = Options()) {
  Solver solver(options);
  load(solver, cnf);
  const Result r = solver.solve();
  if (model) *model = solver.model();
  return r;
}

// Força bruta: testa todas as 2^n atribuições (n até 22). Cada cláusula vira duas
// máscaras de bits: as variáveis que aparecem positivas e as que aparecem negadas.
bool brute_force_sat(const Cnf& cnf) {
  const int n = cnf.numVars;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> masks;
  for (const auto& clause : cnf.clauses) {
    std::uint32_t pos = 0, neg = 0;
    for (int d : clause) (d > 0 ? pos : neg) |= 1u << (std::abs(d) - 1);
    masks.push_back({pos, neg});
  }
  const std::uint32_t full = n == 32 ? 0xFFFFFFFFu : ((1u << n) - 1);
  for (std::uint64_t a = 0; a < (std::uint64_t(1) << n); ++a) {
    const std::uint32_t x = std::uint32_t(a);
    bool ok = true;
    for (const auto& m : masks)
      if (!((x & m.first) | (~x & full & m.second))) {
        ok = false;
        break;
      }
    if (ok) return true;
  }
  return false;
}

Cnf random_3sat(std::mt19937& rng, int n, double ratio) {
  Cnf cnf;
  cnf.numVars = n;
  const int m = int(n * ratio + 0.5);
  for (int i = 0; i < m; ++i) {
    std::vector<int> clause;
    while (clause.size() < 3) {
      const int v = int(rng() % std::uint32_t(n)) + 1;
      bool repeated = false;
      for (int d : clause) repeated = repeated || std::abs(d) == v;
      if (!repeated) clause.push_back(rng() % 2 ? v : -v);
    }
    cnf.add(clause);
  }
  return cnf;
}

SudokuGrid read_sudoku_file(const std::string& name) {
  std::ifstream file(examples_dir + "/" + name);
  std::stringstream text;
  text << file.rdbuf();
  SudokuGrid grid{};
  std::string error;
  if (!parse_sudoku(text.str(), grid, error)) std::cerr << "Sudoku inválido no teste: " << error << "\n";
  return grid;
}

Graph read_graph_file(const std::string& name) {
  std::ifstream file(examples_dir + "/" + name);
  Graph g;
  std::string error;
  if (!read_graph(file, g, error)) std::cerr << "grafo inválido no teste: " << error << "\n";
  return g;
}

std::string grid_text(const SudokuGrid& g) {
  std::string s;
  for (int d : g) s += char('0' + d);
  return s;
}

}  // namespace

TEST(dimacs_valid, "leitor DIMACS: arquivos válidos") {
  const Cnf cnf = parse_ok("c comentário no começo\np cnf 3 3\n1 -3 0 2\n3 -1 0\n\n  -2 0\n");
  CHECK_EQ(cnf.numVars, 3);
  CHECK_EQ(cnf.clauses.size(), std::size_t(3));
  CHECK(cnf.clauses.size() == 3 && cnf.clauses[0] == std::vector<int>({1, -3}));
  CHECK(cnf.clauses.size() == 3 && cnf.clauses[1] == std::vector<int>({2, 3, -1}));  // atravessa linhas
  CHECK(cnf.clauses.size() == 3 && cnf.clauses[2] == std::vector<int>({-2}));
  // Arquivos da SATLIB terminam com "%" e uma linha "0".
  const Cnf satlib = parse_ok("p cnf 2 1\n1 2 0\n%\n0\n");
  CHECK_EQ(satlib.clauses.size(), std::size_t(1));
  // Cláusula vazia é válida (e torna a fórmula insatisfatível).
  const Cnf empty = parse_ok("p cnf 1 1\n0\n");
  CHECK(empty.clauses.size() == 1 && empty.clauses[0].empty());
  // Quebras de linha do Windows.
  CHECK_EQ(parse_ok("p cnf 1 1\r\n1 0\r\n").clauses.size(), std::size_t(1));
}

TEST(dimacs_errors, "leitor DIMACS: erros com o número da linha") {
  CHECK(contains(parse_error(""), "faltou o cabeçalho"));
  CHECK(contains(parse_error("1 2 0\n"), "linha 1: cláusula antes do cabeçalho"));
  CHECK(contains(parse_error("p cnf 2\n"), "linha 1: número de cláusulas inválido"));
  CHECK(contains(parse_error("p dnf 2 1\n"), "o cabeçalho deve ser 'p cnf"));
  CHECK(contains(parse_error("p cnf -1 1\n"), "número de variáveis inválido"));
  CHECK(contains(parse_error("p cnf 2 1 7\n"), "texto sobrando no cabeçalho"));
  CHECK(contains(parse_error("p cnf 2 1\np cnf 2 1\n"), "linha 2: cabeçalho 'p cnf' repetido"));
  CHECK(contains(parse_error("c\np cnf 2 1\n1 3 0\n"), "linha 3: o literal 3 passa do número de variáveis declarado (2)"));
  CHECK(contains(parse_error("p cnf 2 1\n1 -3 0\n"), "o literal -3 passa"));
  CHECK(contains(parse_error("p cnf 2 1\n1 x 0\n"), "linha 2: esperava um número inteiro, encontrei 'x'"));
  CHECK(contains(parse_error("p cnf 2 1\n1 2- 0\n"), "encontrei '2-'"));
  CHECK(contains(parse_error("p cnf 2 1\n99999999999 0\n"), "esperava um número inteiro"));
  CHECK(contains(parse_error("p cnf 2 2\n1 2 0\n"), "o cabeçalho declara 2 cláusulas, mas o arquivo tem 1"));
  CHECK(contains(parse_error("p cnf 2 1\n\n1\n2\n"), "linha 3: a cláusula que começa aqui não termina com 0"));
  CHECK(contains(parse_error("p cnf 2000000000 2000000000\n"), "o limite deste resolvedor"));
}

TEST(hand_written_formulas, "fórmulas feitas à mão") {
  std::vector<LBool> model;
  CHECK(solve_cnf(parse_ok("p cnf 0 0\n")) == Result::Satisfiable);
  CHECK(solve_cnf(parse_ok("p cnf 1 1\n0\n")) == Result::Unsatisfiable);
  CHECK(solve_cnf(parse_ok("p cnf 1 2\n1 0\n-1 0\n")) == Result::Unsatisfiable);
  // Três cláusulas que só deixam x1 = x2 = verdadeiro.
  CHECK(solve_cnf(parse_ok("p cnf 2 3\n1 2 0\n-1 2 0\n1 -2 0\n"), &model) == Result::Satisfiable);
  CHECK(model.size() == 2 && model[0] == LBool::True && model[1] == LBool::True);
  // As 8 cláusulas possíveis sobre 3 variáveis: nenhuma atribuição escapa.
  Cnf all8;
  all8.numVars = 3;
  for (int mask = 0; mask < 8; ++mask) all8.add({mask & 1 ? 1 : -1, mask & 2 ? 2 : -2, mask & 4 ? 3 : -3});
  CHECK(solve_cnf(all8) == Result::Unsatisfiable);
  // Tirando uma, sobra exatamente o modelo que ela proibia.
  Cnf all7 = all8;
  all7.clauses.erase(all7.clauses.begin() + 5);  // proibia x1 = falso, x2 = verdadeiro, x3 = falso
  CHECK(solve_cnf(all7, &model) == Result::Satisfiable);
  CHECK(model.size() == 3 && model[0] == LBool::False && model[1] == LBool::True && model[2] == LBool::False);
  // Cadeia de implicações x1 -> x2 -> ... -> x60 com x1 e não x60: só propagação.
  Cnf chain;
  chain.numVars = 60;
  for (int i = 1; i < 60; ++i) chain.add({-i, i + 1});
  chain.add({1});
  chain.add({-60});
  CHECK(solve_cnf(chain) == Result::Unsatisfiable);
  // Literais repetidos e tautologias são tratados na entrada.
  CHECK(solve_cnf(parse_ok("p cnf 2 3\n1 1 -1 0\n2 2 0\n-2 1 -2 0\n"), &model) == Result::Satisfiable);
  CHECK(model.size() == 2 && model[0] == LBool::True && model[1] == LBool::True);
  // O exemplo do repositório: dois modelos, qualquer um serve.
  std::ifstream file(examples_dir + "/formula.cnf");
  Cnf example;
  std::string error;
  CHECK(read_dimacs(file, example, error));
  CHECK(solve_cnf(example, &model) == Result::Satisfiable);
  CHECK(satisfies(example, model));
}

TEST(pigeonhole_unsat, "casa dos pombos insatisfatível até 8 casas") {
  for (int holes = 1; holes <= 8; ++holes) {
    Solver solver;
    CHECK(load(solver, encode_pigeonhole(holes)) || holes == 1);
    CHECK(solver.solve() == Result::Unsatisfiable);
    if (holes == 8) {
      CHECK(solver.stats().conflicts > 1000);  // prova longa: resolução exige muitos passos
      CHECK(solver.stats().minimizedLiterals > 0);
      CHECK(solver.stats().reductions > 0);
    }
  }
  // Controle: com o mesmo número de pombos e casas a fórmula é satisfatível.
  for (int n = 1; n <= 8; ++n) {
    Cnf cnf;
    cnf.numVars = n * n;
    for (int i = 0; i < n; ++i) {
      std::vector<int> somewhere;
      for (int j = 0; j < n; ++j) somewhere.push_back(i * n + j + 1);
      cnf.add(somewhere);
    }
    for (int j = 0; j < n; ++j)
      for (int a = 0; a < n; ++a)
        for (int b = a + 1; b < n; ++b) cnf.add({-(a * n + j + 1), -(b * n + j + 1)});
    std::vector<LBool> model;
    CHECK(solve_cnf(cnf, &model) == Result::Satisfiable);
    CHECK(satisfies(cnf, model));
  }
}

TEST(random_3sat_brute_force, "3-SAT aleatório conferido por força bruta") {
  std::mt19937 rng(2026);
  int sat = 0, unsat = 0, disagreements = 0, badModels = 0;
  for (int n = 5; n <= 20; ++n) {
    for (int t = 0; t < 12; ++t) {
      const Cnf cnf = random_3sat(rng, n, 4.26);
      std::vector<LBool> model;
      const Result r = solve_cnf(cnf, &model);
      const bool expected = brute_force_sat(cnf);
      if ((r == Result::Satisfiable) != expected || r == Result::Unknown) ++disagreements;
      if (r == Result::Satisfiable) {
        ++sat;
        if (!satisfies(cnf, model)) ++badModels;
      } else {
        ++unsat;
      }
    }
  }
  CHECK_EQ(disagreements, 0);
  CHECK_EQ(badModels, 0);
  CHECK(sat > 20);  // na razão 4,26 as duas respostas aparecem com frequência
  CHECK(unsat > 20);
}

TEST(random_3sat_large, "3-SAT aleatório de 50 a 150 variáveis com modelos conferidos") {
  std::mt19937 rng(4260);
  int sat = 0, unsat = 0, badModels = 0, disagreements = 0;
  for (int n = 50; n <= 150; n += 25) {
    for (int t = 0; t < 10; ++t) {
      const Cnf cnf = random_3sat(rng, n, 4.26);
      std::vector<LBool> model;
      const Result r = solve_cnf(cnf, &model);
      CHECK(r != Result::Unknown);
      if (r == Result::Satisfiable) {
        ++sat;
        if (!satisfies(cnf, model)) ++badModels;
      } else {
        ++unsat;
      }
      // Diferencial entre configurações: Luby e sem minimização dão a mesma resposta.
      Options luby;
      luby.restarts = RestartPolicy::Luby;
      luby.minimize = false;
      std::vector<LBool> other;
      const Result r2 = solve_cnf(cnf, &other, luby);
      if (r2 != r) ++disagreements;
      if (r2 == Result::Satisfiable && !satisfies(cnf, other)) ++badModels;
    }
  }
  CHECK_EQ(badModels, 0);
  CHECK_EQ(disagreements, 0);
  CHECK(sat > 5);
  CHECK(unsat > 5);
}

TEST(incremental_model_count, "uso incremental conta os modelos") {
  // Conta os modelos de fórmulas pequenas bloqueando cada um e compara com a força bruta.
  std::mt19937 rng(7);
  int mismatches = 0;
  for (int t = 0; t < 30; ++t) {
    const int n = 8;
    const Cnf cnf = random_3sat(rng, n, 2.5);
    int expected = 0;
    for (int a = 0; a < (1 << n); ++a) {
      bool ok = true;
      for (const auto& clause : cnf.clauses) {
        bool any = false;
        for (int d : clause) any = any || ((a >> (std::abs(d) - 1)) & 1) == (d > 0 ? 1 : 0);
        ok = ok && any;
      }
      expected += ok;
    }
    Solver solver;
    load(solver, cnf);
    int count = 0;
    std::set<std::vector<LBool>> models;
    while (solver.solve() == Result::Satisfiable && count <= 256) {
      ++count;
      models.insert(solver.model());
      std::vector<Lit> block;
      for (int v = 0; v < n; ++v) block.push_back(make_lit(v, solver.model_true(v)));
      if (!solver.add_clause(block)) break;
    }
    if (count != expected || int(models.size()) != count) ++mismatches;
  }
  CHECK_EQ(mismatches, 0);
}

TEST(sudoku_easy_and_hard, "Sudoku fácil e difícil, com solução única") {
  const SudokuGrid easy = read_sudoku_file("sudoku-facil.txt");
  const SudokuGrid hard = read_sudoku_file("sudoku-dificil.txt");
  const std::string easySolution =
      "534678912672195348198342567859761423426853791713924856961537284287419635345286179";
  const std::string hardSolution =
      "812753649943682175675491283154237896369845721287169534521974368438526917796318452";
  for (const auto& item : {std::make_pair(easy, easySolution), std::make_pair(hard, hardSolution)}) {
    Solver solver;
    load(solver, encode_sudoku(item.first));
    CHECK(solver.solve() == Result::Satisfiable);
    const SudokuGrid solution = decode_sudoku(solver.model());
    CHECK(sudoku_solution_ok(item.first, solution));
    CHECK_EQ(grid_text(solution), item.second);
    // Bloqueando a solução, não sobra outra: ela é única.
    std::vector<Lit> block;
    for (int d : sudoku_blocking_clause(solution)) block.push_back(from_dimacs(d));
    solver.add_clause(block);
    CHECK(solver.solve() == Result::Unsatisfiable);
  }
  // Sem pistas há muitas soluções, e a primeira é válida.
  SudokuGrid blank{};
  std::vector<LBool> model;
  CHECK(solve_cnf(encode_sudoku(blank), &model) == Result::Satisfiable);
  CHECK(sudoku_solution_ok(blank, decode_sudoku(model)));
}

TEST(sudoku_invalid_and_unsolvable, "Sudoku inválido e sem solução") {
  SudokuGrid g{};
  std::string error;
  CHECK(!parse_sudoku("123", g, error) && contains(error, "81 casas, encontrei 3"));
  CHECK(!parse_sudoku(std::string(81, '.') + "1", g, error) && contains(error, "mais de 81"));
  CHECK(!parse_sudoku(std::string(80, '.') + "x", g, error) && contains(error, "caractere inesperado"));
  CHECK(!parse_sudoku("55" + std::string(79, '.'), g, error) && contains(error, "na mesma linha"));
  CHECK(!parse_sudoku("5" + std::string(8, '.') + "5" + std::string(71, '.'), g, error) &&
        contains(error, "na mesma coluna"));
  CHECK(!parse_sudoku("5" + std::string(9, '.') + "5" + std::string(70, '.'), g, error) &&
        contains(error, "no mesmo bloco"));
  // Grade desenhada com separadores também vale.
  CHECK(parse_sudoku("5 3 . | . 7 . | . . .\n" + std::string(72, '0'), g, error));
  // Pistas compatíveis, mas sem solução: a casa (1, 9) só poderia ser 9, e já há
  // um 9 na mesma coluna.
  CHECK(parse_sudoku("12345678." + std::string(8, '.') + "9" + std::string(63, '.'), g, error));
  CHECK(solve_cnf(encode_sudoku(g)) == Result::Unsatisfiable);
}

TEST(queens_count, "N rainhas: contagem com cláusulas de bloqueio") {
  const int expected[] = {0, 1, 0, 0, 2, 10, 4, 40, 92};
  for (int n = 1; n <= 8; ++n) {
    Solver solver;
    load(solver, encode_queens(n));
    std::set<std::vector<int>> solutions;
    int invalid = 0;
    while (solver.solve() == Result::Satisfiable) {
      const std::vector<int> columns = decode_queens(n, solver.model());
      if (!queens_ok(columns)) ++invalid;
      solutions.insert(columns);
      std::vector<Lit> block;
      for (int d : queens_blocking_clause(n, columns)) block.push_back(from_dimacs(d));
      if (!solver.add_clause(block)) break;
    }
    CHECK_EQ(int(solutions.size()), expected[n]);
    CHECK_EQ(invalid, 0);
  }
  CHECK(contains(format_queens({1, 3, 0, 2}), ". D . ."));
}

TEST(graph_coloring, "coloração de grafos") {
  const Graph petersen = read_graph_file("petersen.txt");
  CHECK_EQ(petersen.names.size(), std::size_t(10));
  CHECK_EQ(petersen.edges.size(), std::size_t(15));
  CHECK(solve_cnf(encode_coloring(petersen, 2)) == Result::Unsatisfiable);
  std::vector<LBool> model;
  CHECK(solve_cnf(encode_coloring(petersen, 3), &model) == Result::Satisfiable);
  CHECK(coloring_ok(petersen, 3, decode_coloring(petersen, 3, model)));

  const Graph brasil = read_graph_file("brasil.txt");
  CHECK_EQ(brasil.names.size(), std::size_t(27));
  CHECK(solve_cnf(encode_coloring(brasil, 3)) == Result::Unsatisfiable);
  CHECK(solve_cnf(encode_coloring(brasil, 4), &model) == Result::Satisfiable);
  CHECK(coloring_ok(brasil, 4, decode_coloring(brasil, 4, model)));

  Graph g;
  std::string error;
  std::istringstream triangle("a b\nb c\nc a\na b # repetida\nd\n");
  CHECK(read_graph(triangle, g, error));
  CHECK_EQ(g.names.size(), std::size_t(4));  // d é um vértice isolado
  CHECK_EQ(g.edges.size(), std::size_t(3));
  CHECK(solve_cnf(encode_coloring(g, 2)) == Result::Unsatisfiable);
  CHECK(solve_cnf(encode_coloring(g, 3)) == Result::Satisfiable);
  std::istringstream loop("a a\n");
  CHECK(!read_graph(loop, g, error) && contains(error, "laço"));
  std::istringstream extra("a b c\n");
  CHECK(!read_graph(extra, g, error) && contains(error, "linha 1"));
  std::istringstream empty("# nada\n");
  CHECK(!read_graph(empty, g, error) && contains(error, "vazio"));
  CHECK_EQ(color_name(0), std::string("vermelho"));
  CHECK_EQ(color_name(9), std::string("cor 10"));
}

TEST(competition_output, "saída no formato das competições") {
  std::vector<LBool> model(40, LBool::False);
  model[0] = LBool::True;
  const std::string text = format_model(model, 40);
  CHECK(contains(text, "v 1 -2 -3"));
  CHECK(contains(text, "-40 0\n"));
  std::istringstream lines(text);
  int count = 0;
  for (std::string line; std::getline(lines, line);) {
    ++count;
    CHECK(line.size() <= 80);
    CHECK(line.rfind("v ", 0) == 0);
  }
  CHECK(count >= 2);
  std::ostringstream out;
  write_dimacs(out, parse_ok("p cnf 2 2\n1 -2 0\n2 0\n"), {"teste"});
  CHECK_EQ(out.str(), std::string("c teste\np cnf 2 2\n1 -2 0\n2 0\n"));
}

TEST(conflict_limit, "limite de conflitos devolve indefinido") {
  Options options;
  options.conflictLimit = 50;
  Solver solver(options);
  load(solver, encode_pigeonhole(9));
  CHECK(solver.solve() == Result::Unknown);
  CHECK(solver.stats().conflicts >= 50);
}

int main(int argc, char** argv) {
  if (argc > 1) examples_dir = argv[1];
  return test::run_all();
}
