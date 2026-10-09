// Fórmulas em CNF: leitura e escrita no formato DIMACS e conferência de modelos.
#ifndef SAT_CNF_H
#define SAT_CNF_H

#include <iosfwd>
#include <string>
#include <vector>

#include "solver.h"

namespace sat {

// Fórmula como no arquivo DIMACS: variáveis de 1 a numVars, literal negativo = negado.
struct Cnf {
  int numVars = 0;
  std::vector<std::vector<int>> clauses;

  int new_var() { return ++numVars; }
  void add(std::vector<int> clause) { clauses.push_back(std::move(clause)); }
};

// Lê DIMACS com mensagens de erro em português e o número da linha. Aceita
// comentários (c), cláusulas que atravessam linhas, várias cláusulas por linha e o
// marcador "%" que encerra os arquivos da SATLIB.
bool read_dimacs(std::istream& in, Cnf& out, std::string& error);
void write_dimacs(std::ostream& out, const Cnf& cnf, const std::vector<std::string>& comments = {});

// Carrega a fórmula no resolvedor. Devolve false se ela já for insatisfatível.
bool load(Solver& solver, const Cnf& cnf);

// Toda cláusula tem pelo menos um literal verdadeiro no modelo?
bool satisfies(const Cnf& cnf, const std::vector<LBool>& model);

// Linhas "v ... 0" do formato das competições SAT, com até ~78 caracteres cada.
std::string format_model(const std::vector<LBool>& model, int numVars);

}  // namespace sat

#endif
