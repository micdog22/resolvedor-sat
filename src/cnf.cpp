#include "cnf.h"

#include <algorithm>
#include <climits>
#include <istream>
#include <ostream>
#include <sstream>

namespace sat {
namespace {

constexpr long long kMaxVars = 10000000;  // dez milhões de variáveis

// Converte um token em inteiro, recusando lixo como "12a", "--3" ou estouro.
bool parse_int(const std::string& token, long long& out) {
  if (token.empty()) return false;
  std::size_t i = 0;
  bool negative = false;
  if (token[0] == '-' || token[0] == '+') {
    negative = token[0] == '-';
    i = 1;
  }
  if (i == token.size()) return false;
  long long value = 0;
  for (; i < token.size(); ++i) {
    if (token[i] < '0' || token[i] > '9') return false;
    value = value * 10 + (token[i] - '0');
    if (value > INT_MAX) return false;
  }
  out = negative ? -value : value;
  return true;
}

}  // namespace

bool read_dimacs(std::istream& in, Cnf& out, std::string& error) {
  out = Cnf();
  std::string line;
  int lineNumber = 0;
  bool header = false;
  long long declaredClauses = 0;
  std::vector<int> current;
  int currentStart = 0;  // linha onde a cláusula atual começou

  auto fail = [&](const std::string& message) {
    error = "linha " + std::to_string(lineNumber) + ": " + message;
    return false;
  };

  while (std::getline(in, line)) {
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream is(line);
    std::string token;
    if (!(is >> token)) continue;  // linha em branco
    if (token[0] == 'c') continue;  // comentário
    if (token[0] == '%') break;     // fim dos arquivos da SATLIB

    if (token == "p") {
      if (header) return fail("cabeçalho 'p cnf' repetido");
      std::string format, vars, clauses, extra;
      is >> format >> vars >> clauses;
      long long v = 0, c = 0;
      if (format != "cnf") return fail("o cabeçalho deve ser 'p cnf <variáveis> <cláusulas>'");
      if (!parse_int(vars, v) || v < 0) return fail("número de variáveis inválido no cabeçalho: '" + vars + "'");
      if (v > kMaxVars)
        return fail("o cabeçalho declara " + vars + " variáveis; o limite deste resolvedor é " +
                    std::to_string(kMaxVars));
      if (!parse_int(clauses, c) || c < 0) return fail("número de cláusulas inválido no cabeçalho: '" + clauses + "'");
      if (is >> extra) return fail("texto sobrando no cabeçalho: '" + extra + "'");
      header = true;
      out.numVars = int(v);
      declaredClauses = c;
      // O cabeçalho não é confiável: reserva no máximo um milhão de cláusulas de antemão.
      out.clauses.reserve(std::size_t(std::min<long long>(c, 1 << 20)));
      continue;
    }
    if (!header) return fail("cláusula antes do cabeçalho 'p cnf'");

    do {
      long long lit = 0;
      if (!parse_int(token, lit)) return fail("esperava um número inteiro, encontrei '" + token + "'");
      if (lit == 0) {
        out.clauses.push_back(current);
        current.clear();
        continue;
      }
      if (lit > out.numVars || -lit > out.numVars)
        return fail("o literal " + std::to_string(lit) + " passa do número de variáveis declarado (" +
                    std::to_string(out.numVars) + ")");
      if (current.empty()) currentStart = lineNumber;
      current.push_back(int(lit));
    } while (is >> token);
  }

  if (!header) {
    lineNumber = std::max(lineNumber, 1);
    return fail("faltou o cabeçalho 'p cnf <variáveis> <cláusulas>'");
  }
  if (!current.empty()) {
    lineNumber = currentStart;
    return fail("a cláusula que começa aqui não termina com 0");
  }
  if (static_cast<long long>(out.clauses.size()) != declaredClauses) {
    error = "o cabeçalho declara " + std::to_string(declaredClauses) + " cláusulas, mas o arquivo tem " +
            std::to_string(out.clauses.size());
    return false;
  }
  return true;
}

void write_dimacs(std::ostream& out, const Cnf& cnf, const std::vector<std::string>& comments) {
  for (const std::string& c : comments) out << "c " << c << "\n";
  out << "p cnf " << cnf.numVars << " " << cnf.clauses.size() << "\n";
  for (const std::vector<int>& clause : cnf.clauses) {
    for (int lit : clause) out << lit << " ";
    out << "0\n";
  }
}

bool load(Solver& solver, const Cnf& cnf) {
  solver.reserve_vars(cnf.numVars);
  bool ok = true;
  std::vector<Lit> lits;
  for (const std::vector<int>& clause : cnf.clauses) {
    lits.clear();
    for (int d : clause) lits.push_back(from_dimacs(d));
    ok = solver.add_clause(lits) && ok;
  }
  return ok;
}

bool satisfies(const Cnf& cnf, const std::vector<LBool>& model) {
  for (const std::vector<int>& clause : cnf.clauses) {
    bool sat = false;
    for (int d : clause) {
      const std::size_t v = std::size_t(d > 0 ? d : -d) - 1;
      if (v < model.size() && model[v] == (d > 0 ? LBool::True : LBool::False)) {
        sat = true;
        break;
      }
    }
    if (!sat) return false;
  }
  return true;
}

std::string format_model(const std::vector<LBool>& model, int numVars) {
  std::string out, line = "v";
  for (int v = 1; v <= numVars; ++v) {
    const bool value = std::size_t(v - 1) < model.size() && model[std::size_t(v - 1)] == LBool::True;
    const std::string lit = " " + std::to_string(value ? v : -v);
    if (line.size() + lit.size() > 78) {
      out += line + "\n";
      line = "v";
    }
    line += lit;
  }
  return out + line + " 0\n";
}

}  // namespace sat
