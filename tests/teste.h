// Mini harness de testes: TEST, CHECK, CHECK_EQ, registro automático e resumo no fim.
#ifndef SAT_TESTE_H
#define SAT_TESTE_H

#include <iostream>
#include <string>
#include <vector>

namespace test {

struct Case {
  const char* id;
  const char* description;
  void (*body)();
};

inline std::vector<Case>& cases() {
  static std::vector<Case> list;
  return list;
}
inline int& failures() {
  static int n = 0;
  return n;
}
inline int& checks() {
  static int n = 0;
  return n;
}

struct Registrar {
  Registrar(const char* id, const char* description, void (*body)()) { cases().push_back({id, description, body}); }
};

inline int run_all() {
  for (const Case& c : cases()) {
    const int before = failures();
    c.body();
    std::cout << (failures() == before ? "  ok     " : "  FALHOU ") << c.description << "\n";
  }
  std::cout << "\n" << cases().size() << " testes, " << checks() << " verificações, " << failures() << " falhas\n";
  return failures() == 0 ? 0 : 1;
}

}  // namespace test

#define TEST(id, description)                                         \
  static void id();                                                   \
  static const test::Registrar registrar_##id(#id, description, id);  \
  static void id()

#define CHECK(cond)                                                                    \
  do {                                                                                 \
    ++test::checks();                                                                  \
    if (!(cond)) {                                                                     \
      ++test::failures();                                                              \
      std::cerr << __FILE__ << ":" << __LINE__ << ": falhou: " #cond << std::endl;     \
    }                                                                                  \
  } while (0)

#define CHECK_EQ(a, b)                                                                              \
  do {                                                                                              \
    ++test::checks();                                                                               \
    const auto& valueA_ = (a);                                                                      \
    const auto& valueB_ = (b);                                                                      \
    if (!(valueA_ == valueB_)) {                                                                    \
      ++test::failures();                                                                           \
      std::cerr << __FILE__ << ":" << __LINE__ << ": esperava " #a " == " #b ", obtive " << valueA_ \
                << " e " << valueB_ << std::endl;                                                   \
    }                                                                                               \
  } while (0)

#endif
