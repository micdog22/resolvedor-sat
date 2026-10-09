// Linha de comando: resolve arquivos DIMACS no formato das competições SAT e traz
// codificadores prontos para Sudoku, N rainhas, coloração de grafos e casa dos pombos.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "encoders.h"

using namespace sat;

namespace {

const char* const VERSION = "Resolvedor SAT 1.0";

// Códigos de saída das competições SAT.
constexpr int EXIT_SAT = 10;
constexpr int EXIT_UNSAT = 20;
constexpr int EXIT_UNKNOWN = 0;
constexpr int EXIT_ERROR = 1;
constexpr int EXIT_USAGE = 2;

const char* const USAGE =
    "Uso:\n"
    "  sat [opções] <arquivo.cnf | ->        resolve uma fórmula DIMACS (- lê da entrada padrão)\n"
    "  sat sudoku [arquivo | -] [opções]     resolve um Sudoku 9x9 (0 ou . para casa vazia)\n"
    "  sat rainhas <N> [--todas | --contar]  coloca N rainhas sem ataques entre elas\n"
    "  sat coloracao <arquivo> --cores <K>   colore um grafo dado por lista de arestas\n"
    "  sat pombos <N>                        gera a fórmula da casa dos pombos (N+1 pombos, N casas)\n"
    "  sat ajuda                             mostra esta ajuda\n"
    "\n"
    "Opções:\n"
    "  --estatisticas        mostra estatísticas da busca (linhas começando com c)\n"
    "  --reinicio <tipo>     glucose (padrão, guiado pelo LBD) ou luby\n"
    "  --sem-minimizacao     desliga a minimização recursiva das cláusulas aprendidas\n"
    "  --limite <conflitos>  desiste depois desse número de conflitos (s UNKNOWN)\n"
    "  --sem-modelo          não imprime as linhas v com o modelo\n"
    "  --cnf                 (sudoku, rainhas, coloracao) só imprime a fórmula em DIMACS\n"
    "\n"
    "Códigos de saída: 10 satisfatível, 20 insatisfatível, 0 indefinido, 1 erro na entrada, 2 uso incorreto.\n";

struct Args {
  std::vector<std::string> positional;
  Options options;
  bool stats = false;
  bool printModel = true;
  bool onlyCnf = false;
  bool all = false;
  bool count = false;
  int colors = 0;
};

int usage_error(const std::string& message) {
  std::cerr << "sat: " << message << "\n\n" << USAGE;
  return EXIT_USAGE;
}

bool parse_count(const std::string& text, long long minimum, long long maximum, long long& out) {
  try {
    std::size_t used = 0;
    const long long v = std::stoll(text, &used);
    if (used != text.size() || v < minimum || v > maximum) return false;
    out = v;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

std::string decimal(double value, int places) {
  char text[64];
  std::snprintf(text, sizeof text, "%.*f", places, value);
  std::string s = text;
  for (char& ch : s)
    if (ch == '.') ch = ',';
  return s;
}

bool read_text(const std::string& path, std::string& text, std::string& error) {
  std::ostringstream buffer;
  if (path == "-") {
    buffer << std::cin.rdbuf();
  } else {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
      error = "não consegui abrir '" + path + "'";
      return false;
    }
    buffer << file.rdbuf();
  }
  text = buffer.str();
  return true;
}

void print_stats(const Solver& s, double seconds, std::ostream& out) {
  const Stats& st = s.stats();
  out << "c variáveis: " << s.num_vars() << "  cláusulas originais: " << s.num_clauses() << "\n";
  out << "c conflitos: " << st.conflicts << "  decisões: " << st.decisions << "  propagações: " << st.propagations
      << "\n";
  out << "c reinícios: " << st.restarts << " (bloqueados: " << st.blockedRestarts << ")\n";
  out << "c cláusulas aprendidas: " << st.learnedClauses << " (unitárias: " << st.learnedUnits
      << ")  apagadas: " << st.deletedClauses << "  limpezas: " << st.reductions << "\n";
  if (st.learnedClauses) {
    const double avg = double(st.learnedLiterals) / double(st.learnedClauses);
    const double removed = 100.0 * double(st.minimizedLiterals) / double(st.minimizedLiterals + st.learnedLiterals);
    out << "c tamanho médio das aprendidas: " << decimal(avg, 1) << " literais (a minimização removeu "
        << decimal(removed, 1) << "%)\n";
  }
  out << "c tempo: " << decimal(seconds, 3) << " s\n";
}

// Resolve e cronometra.
Result run_solver(Solver& solver, double& seconds) {
  const auto start = std::chrono::steady_clock::now();
  const Result r = solver.solve();
  seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  return r;
}

int cmd_solve(const Args& args) {
  if (args.positional.size() != 1) return usage_error("informe um arquivo .cnf (ou - para a entrada padrão)");
  const std::string& path = args.positional[0];
  Cnf cnf;
  std::string error;
  bool ok;
  if (path == "-") {
    ok = read_dimacs(std::cin, cnf, error);
  } else {
    std::ifstream file(path);
    if (!file) {
      std::cerr << "sat: não consegui abrir '" << path << "'\n";
      return EXIT_ERROR;
    }
    ok = read_dimacs(file, cnf, error);
  }
  if (!ok) {
    std::cerr << "sat: " << path << ": " << error << "\n";
    return EXIT_ERROR;
  }

  Solver solver(args.options);
  load(solver, cnf);
  double seconds = 0;
  const Result r = run_solver(solver, seconds);
  if (args.stats) {
    std::cout << "c " << VERSION << "\n";
    print_stats(solver, seconds, std::cout);
  }
  if (r == Result::Satisfiable) {
    // Confere o modelo contra a fórmula original antes de anunciar.
    if (!satisfies(cnf, solver.model())) {
      std::cout << "c ERRO INTERNO: o modelo encontrado não satisfaz a fórmula\n";
      return EXIT_ERROR;
    }
    std::cout << "s SATISFIABLE\n";
    if (args.printModel) std::cout << format_model(solver.model(), cnf.numVars);
    return EXIT_SAT;
  }
  if (r == Result::Unsatisfiable) {
    std::cout << "s UNSATISFIABLE\n";
    return EXIT_UNSAT;
  }
  std::cout << "s UNKNOWN\n";
  return EXIT_UNKNOWN;
}

int cmd_sudoku(const Args& args) {
  if (args.positional.size() > 1) return usage_error("sudoku aceita no máximo um arquivo");
  std::string text, error;
  if (!read_text(args.positional.empty() ? "-" : args.positional[0], text, error)) {
    std::cerr << "sat: " << error << "\n";
    return EXIT_ERROR;
  }
  SudokuGrid puzzle{};
  if (!parse_sudoku(text, puzzle, error)) {
    std::cerr << "sat: " << error << "\n";
    return EXIT_ERROR;
  }
  const Cnf cnf = encode_sudoku(puzzle);
  if (args.onlyCnf) {
    write_dimacs(std::cout, cnf, {"Sudoku 9x9: a variável 81*(l-1) + 9*(c-1) + d diz que a casa (l, c) tem o dígito d"});
    return 0;
  }
  Solver solver(args.options);
  load(solver, cnf);
  double seconds = 0;
  Result r = run_solver(solver, seconds);
  if (r != Result::Satisfiable) {
    if (args.stats) print_stats(solver, seconds, std::cout);
    std::cout << (r == Result::Unsatisfiable ? "Este Sudoku não tem solução.\n" : "Não consegui decidir dentro do limite.\n");
    return r == Result::Unsatisfiable ? EXIT_UNSAT : EXIT_UNKNOWN;
  }
  const SudokuGrid solution = decode_sudoku(solver.model());
  if (!sudoku_solution_ok(puzzle, solution)) {
    std::cerr << "sat: erro interno: a solução não respeita as regras\n";
    return EXIT_ERROR;
  }
  // Bloqueia a solução e pede outra: se não existir, ela é única.
  std::vector<Lit> block;
  for (int d : sudoku_blocking_clause(solution)) block.push_back(from_dimacs(d));
  solver.add_clause(block);
  double more = 0;
  const Result second = run_solver(solver, more);
  if (args.stats) print_stats(solver, seconds + more, std::cout);
  std::cout << (second == Result::Unsatisfiable ? "Solução (única):\n" : "Solução (o Sudoku tem mais de uma):\n");
  std::cout << format_sudoku(solution);
  return EXIT_SAT;
}

int cmd_queens(const Args& args) {
  long long n = 0;
  if (args.positional.size() != 1 || !parse_count(args.positional[0], 1, 100, n))
    return usage_error("informe N, o tamanho do tabuleiro (de 1 a 100)");
  const Cnf cnf = encode_queens(int(n));
  if (args.onlyCnf) {
    write_dimacs(std::cout, cnf, {std::to_string(n) + " rainhas: a variável N*l + c + 1 diz que há rainha na linha l, coluna c (contando do zero)"});
    return 0;
  }
  Solver solver(args.options);
  load(solver, cnf);
  double seconds = 0, total = 0;
  long long found = 0;
  for (;;) {
    const Result r = run_solver(solver, seconds);
    total += seconds;
    if (r == Result::Unknown) {
      std::cout << "Não consegui decidir dentro do limite.\n";
      return EXIT_UNKNOWN;
    }
    if (r == Result::Unsatisfiable) break;
    const std::vector<int> columns = decode_queens(int(n), solver.model());
    if (!queens_ok(columns)) {
      std::cerr << "sat: erro interno: rainhas se atacando\n";
      return EXIT_ERROR;
    }
    ++found;
    if (!args.count) {
      if (args.all) std::cout << "Solução " << found << ":\n";
      else std::cout << "Uma solução para " << n << " rainhas (D marca cada rainha):\n";
      std::cout << format_queens(columns);
      if (args.all) std::cout << "\n";
    }
    if (!args.all && !args.count) break;
    // Cláusula de bloqueio: proíbe esta solução e pede a próxima.
    std::vector<Lit> block;
    for (int d : queens_blocking_clause(int(n), columns)) block.push_back(from_dimacs(d));
    if (!solver.add_clause(block)) break;
  }
  if (args.stats) print_stats(solver, total, std::cout);
  if (found == 0) {
    std::cout << "Não existe solução para " << n << " rainhas.\n";
    return EXIT_UNSAT;
  }
  if (args.all || args.count)
    std::cout << "Total: " << found << (found == 1 ? " solução" : " soluções") << " para " << n << " rainhas.\n";
  return EXIT_SAT;
}

int cmd_coloring(const Args& args) {
  if (args.positional.size() != 1) return usage_error("informe o arquivo com as arestas do grafo");
  if (args.colors < 1) return usage_error("informe o número de cores com --cores <K>");
  std::string text, error;
  if (!read_text(args.positional[0], text, error)) {
    std::cerr << "sat: " << error << "\n";
    return EXIT_ERROR;
  }
  std::istringstream in(text);
  Graph graph;
  if (!read_graph(in, graph, error)) {
    std::cerr << "sat: " << args.positional[0] << ": " << error << "\n";
    return EXIT_ERROR;
  }
  const Cnf cnf = encode_coloring(graph, args.colors);
  if (args.onlyCnf) {
    write_dimacs(std::cout, cnf, {"coloração com " + std::to_string(args.colors) + " cores de " +
                                  std::to_string(graph.names.size()) + " vértices"});
    return 0;
  }
  Solver solver(args.options);
  load(solver, cnf);
  double seconds = 0;
  const Result r = run_solver(solver, seconds);
  if (args.stats) print_stats(solver, seconds, std::cout);
  if (r == Result::Unknown) {
    std::cout << "Não consegui decidir dentro do limite.\n";
    return EXIT_UNKNOWN;
  }
  if (r == Result::Unsatisfiable) {
    std::cout << "O grafo (" << graph.names.size() << " vértices, " << graph.edges.size()
              << " arestas) não pode ser colorido com " << args.colors << (args.colors == 1 ? " cor.\n" : " cores.\n");
    return EXIT_UNSAT;
  }
  const std::vector<int> color = decode_coloring(graph, args.colors, solver.model());
  if (!coloring_ok(graph, args.colors, color)) {
    std::cerr << "sat: erro interno: vizinhos com a mesma cor\n";
    return EXIT_ERROR;
  }
  std::cout << "Coloração com " << args.colors << (args.colors == 1 ? " cor" : " cores") << " ("
            << graph.names.size() << " vértices, " << graph.edges.size() << " arestas):\n";
  // Em ordem alfabética, que é mais fácil de conferir.
  std::vector<std::size_t> order(graph.names.size());
  for (std::size_t v = 0; v < order.size(); ++v) order[v] = v;
  std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return graph.names[a] < graph.names[b]; });
  for (std::size_t v : order) std::cout << "  " << graph.names[v] << ": " << color_name(color[v]) << "\n";
  return EXIT_SAT;
}

int cmd_pigeons(const Args& args) {
  long long n = 0;
  if (args.positional.size() != 1 || !parse_count(args.positional[0], 1, 100, n))
    return usage_error("informe N, o número de casas (de 1 a 100)");
  write_dimacs(std::cout, encode_pigeonhole(int(n)),
               {"casa dos pombos: " + std::to_string(n + 1) + " pombos em " + std::to_string(n) + " casas (insatisfatível)",
                "a variável " + std::to_string(n) + "*p + c + 1 diz que o pombo p está na casa c (contando do zero)"});
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> raw(argv + 1, argv + argc);
  if (raw.empty()) return usage_error("faltou o arquivo ou o comando");

  std::string command;
  const std::string first = raw[0];
  if (first == "sudoku" || first == "rainhas" || first == "coloracao" || first == "pombos") {
    command = first;
    raw.erase(raw.begin());
  } else if (first == "ajuda" || first == "--ajuda" || first == "-h" || first == "--help") {
    std::cout << VERSION << ": resolvedor SAT CDCL.\n\n" << USAGE;
    return 0;
  } else if (first == "--versao" || first == "--version") {
    std::cout << VERSION << "\n";
    return 0;
  }

  Args args;
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const std::string& a = raw[i];
    long long value = 0;
    if (a == "--estatisticas") {
      args.stats = true;
    } else if (a == "--sem-modelo") {
      args.printModel = false;
    } else if (a == "--sem-minimizacao") {
      args.options.minimize = false;
    } else if (a == "--cnf") {
      args.onlyCnf = true;
    } else if (a == "--todas") {
      args.all = true;
    } else if (a == "--contar") {
      args.count = true;
    } else if (a == "--reinicio") {
      if (i + 1 >= raw.size()) return usage_error("--reinicio precisa de glucose ou luby");
      const std::string& kind = raw[++i];
      if (kind == "glucose")
        args.options.restarts = RestartPolicy::Glucose;
      else if (kind == "luby")
        args.options.restarts = RestartPolicy::Luby;
      else
        return usage_error("--reinicio aceita glucose ou luby");
    } else if (a == "--limite") {
      if (i + 1 >= raw.size() || !parse_count(raw[++i], 1, 4000000000000LL, value))
        return usage_error("--limite precisa de um número de conflitos");
      args.options.conflictLimit = std::uint64_t(value);
    } else if (a == "--cores" || a == "-k") {
      if (i + 1 >= raw.size() || !parse_count(raw[++i], 1, 1000, value))
        return usage_error("--cores precisa de um número de 1 a 1000");
      args.colors = int(value);
    } else if (a.size() > 1 && a[0] == '-') {
      return usage_error("opção desconhecida: " + a);
    } else {
      args.positional.push_back(a);
    }
  }

  if (command == "sudoku") return cmd_sudoku(args);
  if (command == "rainhas") return cmd_queens(args);
  if (command == "coloracao") return cmd_coloring(args);
  if (command == "pombos") return cmd_pigeons(args);
  return cmd_solve(args);
}
