#include "solver.h"

#include <algorithm>
#include <cmath>

namespace sat {

// ---------------------------------------------------------------------------
// Estruturas auxiliares

void Solver::VarHeap::grow(Var v) {
  if (std::size_t(v) >= index_.size()) index_.resize(std::size_t(v) + 1, -1);
}

void Solver::VarHeap::insert(Var v) {
  grow(v);
  if (contains(v)) return;
  index_[std::size_t(v)] = int(heap_.size());
  heap_.push_back(v);
  sift_up(heap_.size() - 1);
}

void Solver::VarHeap::increased(Var v) {
  if (contains(v)) sift_up(std::size_t(index_[std::size_t(v)]));
}

Var Solver::VarHeap::pop_max() {
  const Var top = heap_[0];
  heap_[0] = heap_.back();
  index_[std::size_t(heap_[0])] = 0;
  index_[std::size_t(top)] = -1;
  heap_.pop_back();
  if (heap_.size() > 1) sift_down(0);
  return top;
}

void Solver::VarHeap::sift_up(std::size_t i) {
  const Var v = heap_[i];
  while (i > 0) {
    const std::size_t parent = (i - 1) / 2;
    if (!before(v, heap_[parent])) break;
    heap_[i] = heap_[parent];
    index_[std::size_t(heap_[i])] = int(i);
    i = parent;
  }
  heap_[i] = v;
  index_[std::size_t(v)] = int(i);
}

void Solver::VarHeap::sift_down(std::size_t i) {
  const Var v = heap_[i];
  for (;;) {
    std::size_t child = 2 * i + 1;
    if (child >= heap_.size()) break;
    if (child + 1 < heap_.size() && before(heap_[child + 1], heap_[child])) ++child;
    if (!before(heap_[child], v)) break;
    heap_[i] = heap_[child];
    index_[std::size_t(heap_[i])] = int(i);
    i = child;
  }
  heap_[i] = v;
  index_[std::size_t(v)] = int(i);
}

void Solver::BoundedQueue::push(std::uint64_t x) {
  if (full()) sum_ -= buffer_[head_];
  buffer_[head_] = x;
  sum_ += x;
  head_ = (head_ + 1) % buffer_.size();
  if (!full()) ++size_;
}

namespace {

// Sequência de Luby (1, 1, 2, 1, 1, 2, 4, 1, 1, 2, ...): reinícios curtos e frequentes,
// com alguns longos de vez em quando. É ótima, a menos de constantes, quando não se
// sabe nada sobre a distribuição do tempo de resolução.
double luby(double y, int x) {
  int size = 1, seq = 0;
  while (size < x + 1) {
    ++seq;
    size = 2 * size + 1;
  }
  while (size - 1 != x) {
    size = (size - 1) >> 1;
    --seq;
    x = x % size;
  }
  return std::pow(y, seq);
}

}  // namespace

// ---------------------------------------------------------------------------
// Variáveis e cláusulas

Solver::Solver(Options options) : opts_(options), order_(activity_) {
  reduceInterval_ = std::uint64_t(opts_.firstReduce);
  nextReduce_ = reduceInterval_;
}

Var Solver::new_var() {
  const Var v = num_vars();
  assigns_.push_back(LBool::Undef);
  level_.push_back(0);
  reason_.push_back(CREF_UNDEF);
  polarity_.push_back(1);  // começa tentando "falso", como o MiniSat
  seen_.push_back(0);
  activity_.push_back(0.0);
  levelStamp_.push_back(0);
  watches_.emplace_back();
  watches_.emplace_back();
  order_.insert(v);
  return v;
}

void Solver::reserve_vars(int count) {
  while (num_vars() < count) new_var();
}

Solver::CRef Solver::alloc_clause(const std::vector<Lit>& lits, bool learnt) {
  CRef cr;
  if (!freeSlots_.empty()) {
    cr = freeSlots_.back();
    freeSlots_.pop_back();
    clauses_[cr] = Clause();
  } else {
    cr = CRef(clauses_.size());
    clauses_.emplace_back();
  }
  Clause& c = clauses_[cr];
  c.lits = lits;
  c.learnt = learnt;
  return cr;
}

void Solver::attach(CRef cr) {
  const Clause& c = clauses_[cr];
  watches_[std::size_t((~c.lits[0]).x)].push_back(Watcher{cr, c.lits[1]});
  watches_[std::size_t((~c.lits[1]).x)].push_back(Watcher{cr, c.lits[0]});
}

// Marca a cláusula como removida. Os vigias são limpos depois, de uma vez só, em
// collect_removed: varrer as listas a cada remoção custaria caro.
void Solver::remove_clause(CRef cr) {
  Clause& c = clauses_[cr];
  if (locked(cr)) reason_[std::size_t(var(c.lits[0]))] = CREF_UNDEF;
  c.removed = true;
  pendingFree_.push_back(cr);
  ++stats_.deletedClauses;
}

void Solver::collect_removed() {
  if (pendingFree_.empty()) return;
  for (std::vector<Watcher>& ws : watches_) {
    ws.erase(std::remove_if(ws.begin(), ws.end(), [&](const Watcher& w) { return clauses_[w.cref].removed; }),
             ws.end());
  }
  for (CRef cr : pendingFree_) {
    std::vector<Lit>().swap(clauses_[cr].lits);
    freeSlots_.push_back(cr);
  }
  pendingFree_.clear();
}

bool Solver::locked(CRef cr) const {
  const Clause& c = clauses_[cr];
  const Lit first = c.lits[0];
  return reason_[std::size_t(var(first))] == cr && value(first) == LBool::True;
}

bool Solver::add_clause(std::vector<Lit> lits) {
  if (!ok_) return false;
  cancel_until(0);
  for (Lit p : lits) reserve_vars(var(p) + 1);

  // Ordena, tira repetidos, descarta tautologias (p e ~p) e literais já falsos.
  std::sort(lits.begin(), lits.end());
  std::size_t j = 0;
  Lit previous = LIT_UNDEF;
  for (std::size_t i = 0; i < lits.size(); ++i) {
    const Lit p = lits[i];
    if (value(p) == LBool::True || p == ~previous) return true;  // já satisfeita
    if (value(p) != LBool::False && p != previous) lits[j++] = previous = p;
  }
  lits.resize(j);

  if (lits.empty()) {
    ok_ = false;
    return false;
  }
  if (lits.size() == 1) {
    enqueue(lits[0], CREF_UNDEF);
    if (propagate() != CREF_UNDEF) ok_ = false;
    return ok_;
  }
  const CRef cr = alloc_clause(lits, false);
  originals_.push_back(cr);
  attach(cr);
  return true;
}

// ---------------------------------------------------------------------------
// Atribuições, propagação e retrocesso

void Solver::enqueue(Lit p, CRef from) {
  const std::size_t v = std::size_t(var(p));
  assigns_[v] = sign(p) ? LBool::False : LBool::True;
  level_[v] = decision_level();
  reason_[v] = from;
  trail_.push_back(p);
}

// Propagação unitária com dois literais vigiados. Cada cláusula vigia dois literais
// não falsos; só quando um deles fica falso a cláusula é visitada, e então procura
// outro literal para vigiar. Se não houver, a cláusula é unitária (o outro vigiado
// é forçado) ou conflitante (o outro também é falso). Retroceder não exige mexer
// nas listas: os vigias continuam válidos.
Solver::CRef Solver::propagate() {
  CRef confl = CREF_UNDEF;
  while (qhead_ < trail_.size()) {
    const Lit p = trail_[qhead_++];
    const Lit falseLit = ~p;
    std::vector<Watcher>& ws = watches_[std::size_t(p.x)];
    ++stats_.propagations;
    std::size_t i = 0, j = 0;
    const std::size_t n = ws.size();
    while (i < n) {
      const Watcher w = ws[i];
      if (value(w.blocker) == LBool::True) {
        ws[j++] = ws[i++];
        continue;
      }
      Clause& c = clauses_[w.cref];
      Lit* lits = c.lits.data();
      if (lits[0] == falseLit) std::swap(lits[0], lits[1]);  // o falso fica em lits[1]
      ++i;
      const Lit first = lits[0];
      const Watcher updated{w.cref, first};
      if (first != w.blocker && value(first) == LBool::True) {
        ws[j++] = updated;
        continue;
      }
      bool moved = false;
      const std::size_t size = c.lits.size();
      for (std::size_t k = 2; k < size; ++k) {
        if (value(lits[k]) != LBool::False) {
          lits[1] = lits[k];
          lits[k] = falseLit;
          watches_[std::size_t((~lits[1]).x)].push_back(updated);
          moved = true;
          break;
        }
      }
      if (moved) continue;
      ws[j++] = updated;
      if (value(first) == LBool::False) {
        confl = w.cref;  // todos os literais falsos: conflito
        qhead_ = trail_.size();
        while (i < n) ws[j++] = ws[i++];
      } else {
        enqueue(first, w.cref);  // cláusula unitária: first é forçado
      }
    }
    ws.resize(j);
  }
  return confl;
}

void Solver::cancel_until(int level) {
  if (decision_level() <= level) return;
  for (std::size_t c = trail_.size(); c-- > std::size_t(trailLim_[std::size_t(level)]);) {
    const Var v = var(trail_[c]);
    assigns_[std::size_t(v)] = LBool::Undef;
    polarity_[std::size_t(v)] = sign(trail_[c]);  // salvamento de fase
    order_.insert(v);
  }
  qhead_ = std::size_t(trailLim_[std::size_t(level)]);
  trail_.resize(qhead_);
  trailLim_.resize(std::size_t(level));
}

Lit Solver::pick_branch_lit() {
  Var next = VAR_UNDEF;
  while (next == VAR_UNDEF || value(next) != LBool::Undef) {
    if (order_.empty()) return LIT_UNDEF;
    next = order_.pop_max();
  }
  return make_lit(next, polarity_[std::size_t(next)] != 0);
}

// ---------------------------------------------------------------------------
// Atividades (VSIDS)

void Solver::bump_var(Var v) {
  if ((activity_[std::size_t(v)] += varInc_) > 1e100) {
    for (double& a : activity_) a *= 1e-100;
    varInc_ *= 1e-100;
  }
  order_.increased(v);
}

void Solver::bump_clause(Clause& c) {
  if ((c.activity += float(clauseInc_)) > 1e20f) {
    for (CRef cr : learnts_) clauses_[cr].activity *= 1e-20f;
    clauseInc_ *= 1e-20;
  }
}

// ---------------------------------------------------------------------------
// Análise de conflitos

// Primeiro UIP: a partir da cláusula em conflito, troca cada literal do nível atual
// pela sua razão (resolução), andando para trás na trilha, até sobrar exatamente um
// literal do nível atual. A negação dele vira o literal "asserting" da cláusula
// aprendida; os demais literais são de níveis anteriores.
void Solver::analyze(CRef confl, std::vector<Lit>& learnt, int& btLevel, std::uint32_t& lbd) {
  int pathCount = 0;
  Lit p = LIT_UNDEF;
  learnt.clear();
  learnt.push_back(LIT_UNDEF);  // lugar do UIP
  std::size_t index = trail_.size();

  do {
    Clause& c = clauses_[confl];
    if (c.learnt) bump_clause(c);
    for (std::size_t k = (p == LIT_UNDEF) ? 0 : 1; k < c.lits.size(); ++k) {
      const Lit q = c.lits[k];
      const Var v = var(q);
      if (!seen_[std::size_t(v)] && level_[std::size_t(v)] > 0) {
        bump_var(v);
        seen_[std::size_t(v)] = 1;
        if (level_[std::size_t(v)] >= decision_level())
          ++pathCount;
        else
          learnt.push_back(q);
      }
    }
    // Próximo literal marcado, andando para trás na trilha.
    while (!seen_[std::size_t(var(trail_[--index]))]) {
    }
    p = trail_[index];
    confl = reason_[std::size_t(var(p))];
    seen_[std::size_t(var(p))] = 0;
    --pathCount;
  } while (pathCount > 0);
  learnt[0] = ~p;

  // Minimização recursiva: um literal é redundante se a sua razão (e as razões das
  // razões) só levam a literais que já estão na cláusula.
  toClear_ = learnt;
  std::size_t j = 1;
  if (opts_.minimize) {
    std::uint32_t abstractLevels = 0;
    for (std::size_t i = 1; i < learnt.size(); ++i) abstractLevels |= abstract_level(var(learnt[i]));
    for (std::size_t i = 1; i < learnt.size(); ++i)
      if (reason_[std::size_t(var(learnt[i]))] == CREF_UNDEF || !lit_redundant(learnt[i], abstractLevels))
        learnt[j++] = learnt[i];
  } else {
    j = learnt.size();
  }
  stats_.minimizedLiterals += learnt.size() - j;
  learnt.resize(j);

  // Nível de retrocesso: o maior nível entre os outros literais, que vai para lits[1]
  // (assim ele é vigiado e a cláusula continua correta depois do salto).
  if (learnt.size() == 1) {
    btLevel = 0;
  } else {
    std::size_t maxI = 1;
    for (std::size_t k = 2; k < learnt.size(); ++k)
      if (level_[std::size_t(var(learnt[k]))] > level_[std::size_t(var(learnt[maxI]))]) maxI = k;
    std::swap(learnt[1], learnt[maxI]);
    btLevel = level_[std::size_t(var(learnt[1]))];
  }
  lbd = compute_lbd(learnt);
  for (Lit q : toClear_) seen_[std::size_t(var(q))] = 0;
}

bool Solver::lit_redundant(Lit p, std::uint32_t abstractLevels) {
  analyzeStack_.clear();
  analyzeStack_.push_back(p);
  const std::size_t top = toClear_.size();
  while (!analyzeStack_.empty()) {
    const CRef r = reason_[std::size_t(var(analyzeStack_.back()))];
    analyzeStack_.pop_back();
    const Clause& c = clauses_[r];
    for (std::size_t k = 1; k < c.lits.size(); ++k) {
      const Lit q = c.lits[k];
      const Var v = var(q);
      if (seen_[std::size_t(v)] || level_[std::size_t(v)] == 0) continue;
      // Só vale a pena descer se q tem razão e está num nível que aparece na cláusula.
      if (reason_[std::size_t(v)] != CREF_UNDEF && (abstract_level(v) & abstractLevels) != 0) {
        seen_[std::size_t(v)] = 1;
        analyzeStack_.push_back(q);
        toClear_.push_back(q);
      } else {
        for (std::size_t t = top; t < toClear_.size(); ++t) seen_[std::size_t(var(toClear_[t]))] = 0;
        toClear_.resize(top);
        return false;
      }
    }
  }
  return true;
}

// LBD (literal block distance): quantos níveis de decisão diferentes aparecem na
// cláusula. Cláusulas com LBD baixo ("cola") ligam poucos blocos de decisões e
// costumam ser as mais úteis.
std::uint32_t Solver::compute_lbd(const std::vector<Lit>& lits) {
  ++stamp_;
  std::uint32_t count = 0;
  for (Lit q : lits) {
    const std::size_t lv = std::size_t(level_[std::size_t(var(q))]);
    if (levelStamp_[lv] != stamp_) {
      levelStamp_[lv] = stamp_;
      ++count;
    }
  }
  return count;
}

// ---------------------------------------------------------------------------
// Limpeza do banco de cláusulas

// Remove cláusulas satisfeitas no nível zero (nunca mais vão propagar nada).
bool Solver::simplify() {
  if (trail_.size() == simplifiedAt_) return true;
  auto satisfied = [&](const Clause& c) {
    for (Lit q : c.lits)
      if (value(q) == LBool::True) return true;
    return false;
  };
  for (std::vector<CRef>* list : {&originals_, &learnts_}) {
    std::size_t j = 0;
    for (CRef cr : *list) {
      if (satisfied(clauses_[cr]))
        remove_clause(cr);
      else
        (*list)[j++] = cr;
    }
    list->resize(j);
  }
  collect_removed();
  simplifiedAt_ = trail_.size();
  return true;
}

// Metade das aprendidas sai, começando pelas de LBD mais alto e menor atividade.
// Cláusulas binárias, de LBD até 2 ("cola") e as que são razão de alguma atribuição
// atual ficam.
void Solver::reduce_db() {
  ++stats_.reductions;
  std::sort(learnts_.begin(), learnts_.end(), [&](CRef a, CRef b) {
    const Clause& x = clauses_[a];
    const Clause& y = clauses_[b];
    const bool xb = x.lits.size() == 2, yb = y.lits.size() == 2;
    if (xb != yb) return yb;
    if (x.lbd != y.lbd) return x.lbd > y.lbd;
    return x.activity < y.activity;
  });
  const std::size_t limit = learnts_.size() / 2;
  std::size_t j = 0;
  for (std::size_t i = 0; i < learnts_.size(); ++i) {
    const CRef cr = learnts_[i];
    const Clause& c = clauses_[cr];
    if (i < limit && c.lits.size() > 2 && c.lbd > 2 && !locked(cr))
      remove_clause(cr);
    else
      learnts_[j++] = cr;
  }
  learnts_.resize(j);
  collect_removed();
}

// ---------------------------------------------------------------------------
// Laço principal

Result Solver::search(std::int64_t conflictBudget) {
  std::int64_t conflictsHere = 0;
  std::vector<Lit>& learnt = learntBuffer_;
  for (;;) {
    const CRef confl = propagate();
    if (confl != CREF_UNDEF) {
      ++stats_.conflicts;
      ++conflictsHere;
      if (decision_level() == 0) return Result::Unsatisfiable;

      int btLevel = 0;
      std::uint32_t lbd = 0;
      analyze(confl, learnt, btLevel, lbd);

      if (opts_.restarts == RestartPolicy::Glucose) {
        // Reinício "bloqueado": se a trilha está bem maior que a média recente, o
        // resolvedor talvez esteja perto de uma solução; melhor não reiniciar agora.
        trailQueue_.push(trail_.size());
        if (stats_.conflicts > 10000 && lbdQueue_.full() && double(trail_.size()) > 1.4 * trailQueue_.average()) {
          lbdQueue_.clear();
          ++stats_.blockedRestarts;
        }
        lbdQueue_.push(lbd);
        lbdSum_ += lbd;
      }

      cancel_until(btLevel);  // retrocesso não cronológico
      ++stats_.learnedClauses;
      stats_.learnedLiterals += learnt.size();
      if (learnt.size() == 1) {
        ++stats_.learnedUnits;
        enqueue(learnt[0], CREF_UNDEF);
      } else {
        const CRef cr = alloc_clause(learnt, true);
        clauses_[cr].lbd = lbd;
        learnts_.push_back(cr);
        attach(cr);
        bump_clause(clauses_[cr]);
        enqueue(learnt[0], cr);
      }
      varInc_ /= opts_.varDecay;
      clauseInc_ /= opts_.clauseDecay;

      if (stats_.conflicts >= nextReduce_) {
        reduceInterval_ += std::uint64_t(opts_.reduceIncrement);
        nextReduce_ = stats_.conflicts + reduceInterval_;
        reduce_db();
      }
    } else {
      bool restart;
      if (opts_.restarts == RestartPolicy::Glucose)
        restart = lbdQueue_.full() && lbdQueue_.average() * 0.8 > double(lbdSum_) / double(stats_.conflicts);
      else
        restart = conflictsHere >= conflictBudget;
      const bool outOfBudget = opts_.conflictLimit && stats_.conflicts >= opts_.conflictLimit;
      if (restart || outOfBudget) {
        lbdQueue_.clear();
        cancel_until(0);
        return Result::Unknown;
      }
      if (decision_level() == 0) simplify();

      const Lit next = pick_branch_lit();
      if (next == LIT_UNDEF) return Result::Satisfiable;  // tudo atribuído sem conflito
      ++stats_.decisions;
      trailLim_.push_back(int(trail_.size()));
      enqueue(next, CREF_UNDEF);
    }
  }
}

Result Solver::solve() {
  model_.clear();
  if (!ok_) return Result::Unsatisfiable;
  Result status = Result::Unknown;
  for (int restarts = 0; status == Result::Unknown; ++restarts) {
    const std::int64_t budget = std::int64_t(luby(2.0, restarts) * opts_.lubyUnit);
    status = search(budget);
    if (status != Result::Unknown) break;
    ++stats_.restarts;
    if (opts_.conflictLimit && stats_.conflicts >= opts_.conflictLimit) break;
  }
  if (status == Result::Satisfiable) model_ = assigns_;
  if (status == Result::Unsatisfiable) ok_ = false;
  cancel_until(0);
  return status;
}

}  // namespace sat
