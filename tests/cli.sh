#!/bin/sh
# Testes da linha de comando: saída no formato das competições, códigos de saída
# (10, 20, 0, 1 e 2) e os subcomandos.
# Uso: sh tests/cli.sh ./build/sat exemplos
set -u
SAT="$1"
EX="$2"
failures=0

# run "descrição" "código esperado" "trecho esperado na saída" comando...
run() {
  desc="$1"
  expected="$2"
  snippet="$3"
  shift 3
  output=$("$@" 2>&1)
  code=$?
  if [ "$code" -ne "$expected" ]; then
    echo "  FALHOU $desc (código $code, esperava $expected)"
    printf '%s\n' "$output" | head -5 | sed 's/^/         | /'
    failures=$((failures + 1))
  elif ! printf '%s\n' "$output" | grep -q -- "$snippet"; then
    echo "  FALHOU $desc (faltou: $snippet)"
    printf '%s\n' "$output" | head -5 | sed 's/^/         | /'
    failures=$((failures + 1))
  else
    echo "  ok     $desc"
  fi
}

TMP=$(mktemp -d "${TMPDIR:-/tmp}/sat-cli.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

run "fórmula satisfatível" 10 "^s SATISFIABLE$" "$SAT" "$EX/formula.cnf"
run "linha v termina com 0" 10 "^v .* 0$" "$SAT" "$EX/formula.cnf"
"$SAT" pombos 7 > "$TMP/pombos.cnf"
run "casa dos pombos insatisfatível" 20 "^s UNSATISFIABLE$" "$SAT" "$TMP/pombos.cnf"
run "estatísticas em linhas c" 20 "^c conflitos: " "$SAT" --estatisticas --reinicio luby "$TMP/pombos.cnf"
run "entrada padrão" 20 "^s UNSATISFIABLE$" sh -c "\"$SAT\" - < \"$TMP/pombos.cnf\""
"$SAT" pombos 10 > "$TMP/pombos10.cnf"
run "limite de conflitos" 0 "^s UNKNOWN$" "$SAT" --limite 100 "$TMP/pombos10.cnf"
run "sem modelo" 10 "^s SATISFIABLE$" "$SAT" --sem-modelo "$EX/formula.cnf"
if "$SAT" --sem-modelo "$EX/formula.cnf" | grep -q "^v "; then
  echo "  FALHOU --sem-modelo ainda imprimiu linhas v"
  failures=$((failures + 1))
fi

printf 'p cnf 2 1\n1 3 0\n' > "$TMP/ruim.cnf"
run "erro de leitura com linha" 1 "linha 2: o literal 3" "$SAT" "$TMP/ruim.cnf"
run "arquivo inexistente" 1 "não consegui abrir" "$SAT" "$TMP/missing.cnf"
run "sem argumentos" 2 "Uso:" "$SAT"
run "opção desconhecida" 2 "opção desconhecida" "$SAT" --xyz "$EX/formula.cnf"
run "ajuda" 0 "sat rainhas" "$SAT" ajuda

run "sudoku fácil" 10 "Solução (única)" "$SAT" sudoku "$EX/sudoku-facil.txt"
run "sudoku difícil" 10 "8 1 2 | 7 5 3 | 6 4 9" "$SAT" sudoku "$EX/sudoku-dificil.txt"
run "sudoku pela entrada padrão" 10 "5 3 4 | 6 7 8 | 9 1 2" sh -c "\"$SAT\" sudoku < \"$EX/sudoku-facil.txt\""
"$SAT" sudoku --cnf "$EX/sudoku-dificil.txt" > "$TMP/sudoku.cnf"
run "sudoku em DIMACS resolvido de novo" 10 "^s SATISFIABLE$" "$SAT" "$TMP/sudoku.cnf"
printf '55' > "$TMP/ruim.txt"
run "sudoku inválido" 1 "81 casas" "$SAT" sudoku "$TMP/ruim.txt"

run "8 rainhas" 10 "D marca cada rainha" "$SAT" rainhas 8
run "92 soluções" 10 "Total: 92 soluções para 8 rainhas." "$SAT" rainhas 8 --contar
run "4 rainhas, todas" 10 "Solução 2:" "$SAT" rainhas 4 --todas
run "3 rainhas não têm solução" 20 "Não existe solução para 3 rainhas." "$SAT" rainhas 3
run "rainhas sem N" 2 "informe N" "$SAT" rainhas

run "Brasil não sai com 3 cores" 20 "não pode ser colorido com 3 cores" "$SAT" coloracao "$EX/brasil.txt" --cores 3
run "Brasil com 4 cores" 10 "MG: " "$SAT" coloracao "$EX/brasil.txt" --cores 4
run "Petersen com 3 cores" 10 "Coloração com 3 cores" "$SAT" coloracao "$EX/petersen.txt" -k 3
run "coloração sem cores" 2 "--cores" "$SAT" coloracao "$EX/petersen.txt"

if [ "$failures" -ne 0 ]; then
  echo "cli.sh: $failures falhas"
  exit 1
fi
echo "cli.sh: todos os testes de linha de comando passaram"
