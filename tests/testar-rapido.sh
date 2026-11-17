#!/bin/bash
# A bateria completa (tests/testar.sh) em duas metades ao mesmo tempo: a parte principal e a parte extra rodam em
# paralelo (portas e discos diferentes), e no fim todas as telas e verificacoes sao conferidas juntas, com a mesma
# regra da bateria normal (grava teste/aprovado so se tudo passar). Uso: bash tests/testar-rapido.sh [--referencias]
TOS=$(cd "$(dirname "$0")/.." && pwd); cd "$TOS"
mkdir -p teste; rm -f teste/z_*.ppm teste/aprovado
INICIO=$(date +%s)
( PARTE=principal bash tests/testar.sh > teste/rapido-principal.txt 2>&1 ) &
( PARTE=extras bash tests/testar.sh > teste/rapido-extras.txt 2>&1 ) &
wait
cat teste/resumo-principal.txt teste/resumo-extras.txt > teste/resumo.txt
echo "bateria em $(( ($(date +%s) - INICIO) / 60 )) min"
if /usr/bin/python3 "$TOS/tests/conferir.py" "$TOS/teste" "$TOS/tests/referencias" "$1"; then
  sed -n 's/^BUILD_ID=//p' output/x86_64/target/etc/os-release > teste/aprovado
  echo "APROVADO: compilacao $(cat teste/aprovado) pode ser publicada"
else
  echo "REPROVADO: corrija antes de publicar"
fi
