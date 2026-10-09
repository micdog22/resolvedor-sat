// Resolvedor CDCL: dois literais vigiados, VSIDS, aprendizado pelo primeiro UIP com
// minimização recursiva, retrocesso não cronológico, reinícios (Luby ou Glucose),
// salvamento de fase e limpeza das cláusulas aprendidas pelo LBD.
#ifndef SAT_SOLVER_H
#define SAT_SOLVER_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace sat {

using Var = int;
constexpr Var VAR_UNDEF = -1;

// Literal: 2 * variável + sinal (sinal 1 = negado). Assim ~p é só um XOR com 1.
struct Lit {
  int x;
  bool operator==(Lit o) const { return x == o.x; }
  bool operator!=(Lit o) const { return x != o.x; }
  bool operator<(Lit o) const { return x < o.x; }
};

inline Lit make_lit(Var v, bool negated = false) { return Lit{2 * v + (negated ? 1 : 0)}; }
inline Lit operator~(Lit p) { return Lit{p.x ^ 1}; }
inline Var var(Lit p) { return p.x >> 1; }
inline bool sign(Lit p) { return (p.x & 1) != 0; }
constexpr Lit LIT_UNDEF{-2};

// DIMACS numera as variáveis a partir de 1 e usa o sinal de menos para a negação.
inline Lit from_dimacs(int d) { return make_lit(std::abs(d) - 1, d < 0); }
inline int to_dimacs(Lit p) { return sign(p) ? -(var(p) + 1) : var(p) + 1; }

enum class LBool : std::uint8_t { True = 0, False = 1, Undef = 2 };

enum class Result { Satisfiable, Unsatisfiable, Unknown };
enum class RestartPolicy { Luby, Glucose };

struct Options {
  RestartPolicy restarts = RestartPolicy::Glucose;
  double varDecay = 0.95;        // VSIDS: quanto a atividade antiga "esquece" a cada conflito
  double clauseDecay = 0.999;
  int lubyUnit = 100;            // conflitos por unidade da sequência de Luby
  std::uint64_t conflictLimit = 0;  // 0: sem limite
  bool minimize = true;          // minimização recursiva das cláusulas aprendidas
  int firstReduce = 2000;        // conflitos até a primeira limpeza das aprendidas
  int reduceIncrement = 300;     // o intervalo entre limpezas cresce isso a cada vez
};

struct Stats {
  std::uint64_t decisions = 0;
  std::uint64_t propagations = 0;
  std::uint64_t conflicts = 0;
  std::uint64_t restarts = 0;
  std::uint64_t learnedClauses = 0;
  std::uint64_t learnedUnits = 0;
  std::uint64_t learnedLiterals = 0;    // depois da minimização
  std::uint64_t minimizedLiterals = 0;  // removidos pela minimização
  std::uint64_t deletedClauses = 0;
  std::uint64_t reductions = 0;
  std::uint64_t blockedRestarts = 0;
};

class Solver {
 public:
  explicit Solver(Options options = Options());

  Var new_var();
  void reserve_vars(int count);  // garante pelo menos count variáveis
  int num_vars() const { return int(assigns_.size()); }
  int num_clauses() const { return int(originals_.size()); }
  int num_learnts() const { return int(learnts_.size()); }

  // Adiciona uma cláusula. Pode ser chamada de novo depois de solve(), por exemplo
  // para bloquear uma solução e procurar a próxima. Devolve false se a fórmula
  // ficou insatisfatível já no nível zero.
  bool add_clause(std::vector<Lit> lits);

  Result solve();

  // Valor de cada variável no último modelo encontrado (depois de Satisfiable).
  const std::vector<LBool>& model() const { return model_; }
  bool model_true(Var v) const { return model_[std::size_t(v)] == LBool::True; }
  const Stats& stats() const { return stats_; }

 private:
  using CRef = std::uint32_t;
  static constexpr CRef CREF_UNDEF = 0xFFFFFFFFu;

  struct Clause {
    std::vector<Lit> lits;  // lits[0] e lits[1] são os vigiados
    float activity = 0;
    std::uint32_t lbd = 0;
    bool learnt = false;
    bool removed = false;
  };

  // Na lista de vigias de p ficam as cláusulas que vigiam ~p: quando p vira
  // verdadeiro, são elas que precisam de atenção. O "bloqueador" é um literal da
  // cláusula; se ele já é verdadeiro, nem é preciso abrir a cláusula.
  struct Watcher {
    CRef cref;
    Lit blocker;
  };

  // Heap binário de variáveis ordenado pela atividade VSIDS.
  class VarHeap {
   public:
    explicit VarHeap(const std::vector<double>& activity) : activity_(activity) {}
    bool empty() const { return heap_.empty(); }
    bool contains(Var v) const { return std::size_t(v) < index_.size() && index_[std::size_t(v)] >= 0; }
    void grow(Var v);
    void insert(Var v);
    void increased(Var v);  // a atividade de v subiu
    Var pop_max();

   private:
    bool before(Var a, Var b) const { return activity_[std::size_t(a)] > activity_[std::size_t(b)]; }
    void sift_up(std::size_t i);
    void sift_down(std::size_t i);
    const std::vector<double>& activity_;
    std::vector<Var> heap_;
    std::vector<int> index_;
  };

  // Fila circular de tamanho fixo com a soma dos elementos (reinícios do Glucose).
  class BoundedQueue {
   public:
    explicit BoundedQueue(std::size_t capacity) : buffer_(capacity, 0) {}
    void push(std::uint64_t x);
    bool full() const { return size_ == buffer_.size(); }
    double average() const { return size_ ? double(sum_) / double(size_) : 0.0; }
    void clear() { size_ = head_ = 0; sum_ = 0; }

   private:
    std::vector<std::uint64_t> buffer_;
    std::size_t size_ = 0, head_ = 0;
    std::uint64_t sum_ = 0;
  };

  LBool value(Var v) const { return assigns_[std::size_t(v)]; }
  LBool value(Lit p) const {
    const LBool a = assigns_[std::size_t(var(p))];
    return a == LBool::Undef ? LBool::Undef : LBool(std::uint8_t(a) ^ std::uint8_t(sign(p)));
  }
  int decision_level() const { return int(trailLim_.size()); }
  std::uint32_t abstract_level(Var v) const { return 1u << (level_[std::size_t(v)] & 31); }

  CRef alloc_clause(const std::vector<Lit>& lits, bool learnt);
  void attach(CRef cr);
  void remove_clause(CRef cr);
  void collect_removed();
  bool locked(CRef cr) const;

  void enqueue(Lit p, CRef from);
  CRef propagate();
  void analyze(CRef confl, std::vector<Lit>& learnt, int& btLevel, std::uint32_t& lbd);
  bool lit_redundant(Lit p, std::uint32_t abstractLevels);
  std::uint32_t compute_lbd(const std::vector<Lit>& lits);
  void cancel_until(int level);
  Lit pick_branch_lit();
  Result search(std::int64_t conflictBudget);
  bool simplify();
  void reduce_db();

  void bump_var(Var v);
  void bump_clause(Clause& c);

  Options opts_;
  Stats stats_;
  bool ok_ = true;

  std::vector<Clause> clauses_;
  std::vector<CRef> freeSlots_, pendingFree_;
  std::vector<CRef> originals_, learnts_;
  std::vector<std::vector<Watcher>> watches_;  // indexado pelo literal

  std::vector<LBool> assigns_;
  std::vector<int> level_;
  std::vector<CRef> reason_;
  std::vector<char> polarity_;  // fase salva: true = negado
  std::vector<char> seen_;
  std::vector<double> activity_;
  VarHeap order_;
  double varInc_ = 1.0;
  double clauseInc_ = 1.0;

  std::vector<Lit> trail_;
  std::vector<int> trailLim_;
  std::size_t qhead_ = 0;
  std::size_t simplifiedAt_ = 0;

  std::vector<Lit> analyzeStack_, toClear_, learntBuffer_;
  std::vector<std::uint64_t> levelStamp_;
  std::uint64_t stamp_ = 0;

  BoundedQueue lbdQueue_{50};
  BoundedQueue trailQueue_{5000};
  std::uint64_t lbdSum_ = 0;
  std::uint64_t nextReduce_ = 0;
  std::uint64_t reduceInterval_ = 0;

  std::vector<LBool> model_;
};

}  // namespace sat

#endif
