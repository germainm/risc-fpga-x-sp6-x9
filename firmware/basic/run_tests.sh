#!/bin/bash
# Tests du Tiny BASIC sur hote. Chaque test: script -> sortie attendue (grep).
PASS=0; FAIL=0
t() {  # t "nom" "script" "attendu_grep" [attendu2...]
  local name="$1" script="$2"; shift 2
  local out=$(printf "%b" "$script" | ./basic_host 2>&1 | grep -v '^> ')
  local ok=1
  for exp in "$@"; do
    echo "$out" | grep -qF -- "$exp" || { ok=0; echo "ECHEC [$name]: manque <$exp>"; echo "$out" | tail -8 | sed 's/^/    /'; }
  done
  [ $ok = 1 ] && PASS=$((PASS+1)) || FAIL=$((FAIL+1))
}
tn() { # test negatif: la chaine NE doit PAS apparaitre
  local name="$1" script="$2" bad="$3"
  local out=$(printf "%b" "$script" | ./basic_host 2>&1 | grep -v '^> ')
  if echo "$out" | grep -qF -- "$bad"; then FAIL=$((FAIL+1)); echo "ECHEC [$name]: <$bad> present"; else PASS=$((PASS+1)); fi
}

t "arith1"   "PRINT 1+2*3\n" "7"
t "arith2"   "PRINT (1+2)*3\n" "9"
t "hex"      "PRINT -5+\$10\n" "11"
t "div"      "PRINT 10/3\n" "3"
t "mod"      "PRINT 7%3\n" "1"
t "assoc"    "PRINT 2-3-4\n" "-5"
t "negnum"   "PRINT -(2+3)*4\n" "-20"
t "vars"     "A=6 : PRINT A*A\n" "36"
t "let"      "LET B=7\nPRINT B+1\n" "8"
t "strcat"   "A\$=\"HELLO\"+\" X\"\nPRINT A\$\n" "HELLO X"
t "len"      "A\$=\"HELLO X\"\nPRINT LEN(A\$)\n" "7"
t "lenlit"   "PRINT LEN(\"ABC\")\n" "3"
t "for"      "10 FOR I=1 TO 3\n20 PRINT I;\n30 NEXT\nRUN\n" "123"
t "forstep"  "10 FOR I=6 TO 2 STEP -2\n20 PRINT I;\n30 NEXT I\nRUN\n" "642"
t "fornest"  "10 FOR I=1 TO 2\n20 FOR J=1 TO 2\n30 PRINT I*10+J;\n40 NEXT J\n50 NEXT I\nRUN\n" "11122122"
t "gosub"    "10 GOSUB 100\n20 PRINT \"B\"\n30 END\n100 PRINT \"A\";\n110 RETURN\nRUN\n" "AB"
t "ifnum"    "10 A=5\n20 IF A>3 THEN PRINT \"GT\"\n30 IF A=5 THEN PRINT \"EQ\"\n40 IF A<>5 THEN PRINT \"NE\"\nRUN\n" "GT" "EQ"
tn "ifnum2"  "10 A=5\n40 IF A<>5 THEN PRINT \"NE\"\nRUN\n" "NE"
t "ifstr"    "10 A\$=\"HI\"\n20 IF A\$=\"HI\" THEN PRINT \"YES\"\n30 IF A\$<>\"HO\" THEN PRINT \"DIFF\"\nRUN\n" "YES" "DIFF"
t "ifgoto"   "10 A=1\n20 IF A=1 THEN 100\n30 PRINT \"NO\"\n40 END\n100 PRINT \"JUMPED\"\nRUN\n" "JUMPED"
tn "ifgoto2" "10 A=1\n20 IF A=1 THEN 100\n30 PRINT \"NO\"\n40 END\n100 PRINT \"JUMPED\"\nRUN\n" "NO"
t "goto"     "10 GOTO 40\n20 PRINT \"SKIP\"\n40 PRINT \"OK40\"\nRUN\n" "OK40"
t "colon"    "10 A=2 : B=3 : PRINT A+B\nRUN\n" "5"
t "replace"  "10 PRINT \"X\"\n10 PRINT \"Y\"\nLIST\n" "10 PRINT \"Y\""
tn "replace2" "10 PRINT \"X\"\n10 PRINT \"Y\"\nLIST\n" "\"X\""
t "delete"   "10 PRINT \"X\"\n20 PRINT \"Z\"\n10\nLIST\n" "20 PRINT \"Z\""
t "sortins"  "30 PRINT \"C\";\n10 PRINT \"A\";\n20 PRINT \"B\";\nRUN\n" "ABC"
t "inputn"   "10 INPUT A\n20 PRINT A*2\nRUN\n21\n" "42"
t "inputs"   "10 INPUT A\$\n20 PRINT \"<\";A\$;\">\"\nRUN\nsalut\n" "<salut>"
t "inputp"   "10 INPUT \"AGE\";A\n20 PRINT A+1\nRUN\n41\n" "AGE? " "42"
t "noline"   "10 GOTO 999\nRUN\n" "LIGNE INTROUVABLE EN 10"
t "retwo"    "RETURN\n" "RETURN SANS GOSUB"
t "nextwo"   "NEXT\n" "NEXT SANS FOR"
t "div0"     "PRINT 1/0\n" "DIVISION PAR ZERO"
t "poke"     "POKE \$80000024,255\nPRINT PEEK(\$80000024)\n" "255"
t "ledbell"  "LED \$FFF\nBELL 440\nBELL 0\n" "[LED FFF]" "[BELL 440]" "[BELL 0]"
t "seg"      "SEG 0,15\nSEG 1,-1\n" "[SEG 0=15]" "[SEG 1=-1]"
t "dipfn"    "PRINT DIP()\n" "170"
t "free"     "FREE\n" "OCTETS LIBRES"
t "printsem" "PRINT \"A\";\"B\"\n" "AB"
t "shortpr"  "?\"HI\"\n" "HI"
t "runclear" "10 PRINT A\nA=9\nRUN\n" "0"
t "rnd"      "10 FOR I=1 TO 20\n20 A=RND(6)\n30 IF A<0 THEN PRINT \"BAD\"\n40 IF A>5 THEN PRINT \"BAD\"\n50 NEXT\n60 PRINT \"RNDOK\"\nRUN\n" "RNDOK"
tn "rnd2"    "10 FOR I=1 TO 20\n20 A=RND(6)\n30 IF A<0 THEN PRINT \"BAD\"\n40 IF A>5 THEN PRINT \"BAD\"\n50 NEXT\nRUN\n" "BAD"
t "abs"      "PRINT ABS(-42)\n" "42"
t "saveload" "10 PRINT \"PERSIST\"\nSAVE T1\nNEW\nLIST\nLOAD T1\nRUN\n" "SAUVE: T1.BAS" "CHARGE: T1.BAS" "PERSIST"
t "rem"      "10 REM RIEN : PRINT \"NON\"\n20 PRINT \"OUI\"\nRUN\n" "OUI"
tn "rem2"    "10 REM PRINT \"NON\"\n20 PRINT \"OUI\"\nRUN\n" "NON"
t "strassign" "A\$=\"UN\"\nB\$=A\$+\"+DEUX\"\nPRINT B\$\n" "UN+DEUX"
t "toolong"  "A\$=\"0123456789012345678901234567890123456789\"+\"X\"\n" "CHAINE TROP LONGUE"
t "bignum"   "PRINT 2000000000+147483647\n" "2147483647"

echo "=================================="
echo "REUSSIS: $PASS   ECHECS: $FAIL"
rm -f T1.BAS

echo "--- LCD ---"
t "lcdinit" "LCDINIT\n" "[LCD INIT]"
t "lcdclr"  "LCDCLR\n" "[LCD CLR]"
t "lcdpos"  "LCDPOS 1,5\n" "[LCD POS 1,5]"
t "lcdprint" "LCDPRINT \"HI \";42\n" "[LCD:HI ]" "[LCD:4][LCD:2]"
t "lcdvar"  "A\$=\"YO\"\nLCDPRINT A\$\n" "[LCD:YO]"
t "lcdfull" "LCDINIT\nLCDCLR\nLCDPOS 0,0\nLCDPRINT \"BONJOUR\"\nLCDPOS 1,0\nLCDPRINT \"RISC-V\"\n" "[LCD INIT]" "[LCD CLR]" "[LCD POS 0,0]" "[LCD:BONJOUR]" "[LCD POS 1,0]" "[LCD:RISC-V]"
echo "REUSSIS: $PASS   ECHECS: $FAIL"
