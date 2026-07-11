# Tiny BASIC RISC-V — Guide de référence

Interpréteur BASIC pour le SoC PicoRV32 sur carte X-SP6-X9 (Spartan-6 XC6SLX9).
Console série 115200 8N1. Chaque ligne tapée sans numéro s'exécute
immédiatement (mode direct) ; avec un numéro, elle est stockée dans le
programme.

---

## Aide-mémoire

| Catégorie | Commandes |
|---|---|
| Langage | `PRINT` (ou `?`), `INPUT "prompt";A`, `LET`, `IF/THEN`, `GOTO`, `GOSUB/RETURN`, `FOR/TO/STEP/NEXT`, `REM`, `END`, `:` |
| Variables | `A`-`Z` (entiers 32 bits), `A$`-`Z$` (chaînes, 40 car. max, concat `+`) |
| Fonctions | `PEEK(a)` `DIP()` `BTN()` `INKEY()` `RND(n)` `ABS(n)` `LEN(s$)` |
| Matériel | `POKE a,v` · `LED v` · `BELL hz` · `SEG d,v` · `DELAY ms` · `CLS` · hexa `$FFF` |
| LCD1602 | `LCDINIT` · `LCDCLR` · `LCDPOS ligne,colonne` · `LCDPRINT expr` |
| Direct | `LIST` `RUN` `NEW` `FREE` `DIR`/`FILES` `SAVE nom` `LOAD nom` `BYE` — **Ctrl-C** interrompt |

---

## 1. Variables

- **Numériques `A` à `Z`** — entiers signés 32 bits. Initialisées à 0 par `RUN`/`NEW`.
- **Chaînes `A$` à `Z$`** — 40 caractères maximum, buffer fixe (pas d'allocation
  dynamique). `A` et `A$` sont deux variables totalement distinctes.
- Affectation avec ou sans `LET` : `LET A=5` et `A=5` sont équivalents.
- Concaténation de chaînes avec `+` : `A$="BON"+"JOUR"`.

## 2. Littéraux

- Décimal : `42`, `-7`
- Hexadécimal : `$FF`, `$8000003C` (utile pour les adresses MMIO)
- Chaîne : `"texte entre guillemets"`

## 3. Opérateurs

| Opérateur | Sens |
|---|---|
| `+ - * /` | arithmétique (division entière) |
| `%` | modulo |
| `= <> < > <= >=` | comparaison (nombres et chaînes) |

Les parenthèses sont supportées : `(1+2)*3`.

## 4. Instructions de langage

### PRINT (ou `?`)
Affiche des nombres, chaînes ou variables séparés par `;` (pas d'espace) ou
`,` (tabulation à la colonne suivante multiple de 8). Une virgule finale
sans rien après revient à la ligne normalement ; un `;` final supprime le
retour à la ligne.

```basic
PRINT "X="; X
? "A", "B", "C"
```

### INPUT
Lit un nombre ou une chaîne depuis la console, avec invite optionnelle.

```basic
INPUT A            ' affiche "? " et attend un nombre
INPUT "AGE";A      ' affiche "AGE? "
INPUT NOM$         ' lit une chaîne
```

### LET
`LET A = expression` — le mot-clé `LET` est optionnel.

### IF / THEN
```basic
IF A > 3 THEN PRINT "GRAND"
IF A$ = "OUI" THEN 100      ' saut direct vers la ligne 100
```
Une seule instruction après `THEN` (ou un numéro de ligne). Pas de `ELSE`.

### GOTO / GOSUB / RETURN
```basic
GOTO 100
GOSUB 500
...
500 PRINT "SOUS-PROGRAMME"
510 RETURN
```
16 niveaux de `GOSUB` imbriqués maximum.

### FOR / TO / STEP / NEXT
```basic
FOR I = 1 TO 10
  PRINT I
NEXT I               ' le nom de variable apres NEXT est optionnel

FOR I = 10 TO 1 STEP -2
  PRINT I
NEXT
```
8 niveaux de boucles imbriquées maximum.

### REM
Ligne de commentaire, ignorée à l'exécution. `REM` ignore aussi tout ce qui
suit sur la même ligne, y compris un `:`.

### END / STOP
Arrête l'exécution du programme et revient à l'invite `>`.

### Séparateur `:`
Plusieurs instructions sur une même ligne :
```basic
10 A=1 : B=2 : PRINT A+B
```

## 5. Fonctions

| Fonction | Résultat |
|---|---|
| `PEEK(addr)` | lit le mot 32 bits à l'adresse `addr` |
| `ABS(n)` | valeur absolue |
| `RND(n)` | entier aléatoire entre 0 et n-1 |
| `LEN(s$)` | longueur de la chaîne |
| `DIP()` | état brut des 8 interrupteurs DIP |
| `BTN()` | état des boutons : bit0=K1, bit1=K2, bit2=K4 |
| `INKEY()` | code de la touche en attente sur la console, -1 si aucune (non bloquant) |

## 6. Accès au matériel

Le SoC expose ses périphériques par registres mémoire (MMIO). `POKE`/`PEEK`
donnent un accès complet et direct ; les mots-clés ci-dessous sont du sucre
syntaxique pour les usages courants.

| Instruction | Effet |
|---|---|
| `POKE addr, valeur` | écrit un mot 32 bits à `addr` |
| `LED valeur` | 12 LEDs D3-D14 ; bit 0 = D3 … bit 11 = D14 |
| `BELL hz` | tonalité continue à `hz` Hz ; `BELL 0` = silence |
| `SEG chiffre, valeur` | affiche `valeur` (0-15, hexa) sur le digit 0-7 de l'afficheur 7 segments ; `valeur=-1` éteint le digit |
| `DELAY ms` | pause de `ms` millisecondes (interruptible par Ctrl-C) |
| `CLS` | efface l'écran du terminal (séquence ANSI) |

Adresses MMIO utiles pour `PEEK`/`POKE` (voir `soc.h` pour la liste complète) :

| Adresse | Registre |
|---|---|
| `$80000000` | UART données |
| `$80000004` | UART statut |
| `$80000024` | LEDs (12 bits) |
| `$80000028` | Boutons K1/K2/K4 |
| `$8000002C` | I2C bus 1 (EEPROM 24C04, P101/P102) |
| `$80000030` | GPIO LCD |
| `$80000038` | Cloche |
| `$8000003C` | DIP switch |
| `$80000040`-`$8000005C` | Afficheur 7 segments (digits 0-7) |
| `$80000060` | I2C bus 2 (capteurs externes, P5/P7) |
| `$80000064` | GPIO générique : direction (0x64), sortie (0x68), entrée (0x6C) |

## 7. Écran LCD1602

Un écran LCD1602 (HD44780, mode 8 bits) est piloté par 4 instructions dédiées.
`LCDINIT` doit être exécuté une seule fois avant tout autre appel LCD.

| Instruction | Effet |
|---|---|
| `LCDINIT` | initialise l'écran (une fois, en début de programme) |
| `LCDCLR` | efface l'écran et replace le curseur en haut à gauche |
| `LCDPOS ligne, colonne` | place le curseur (ligne 0 ou 1, colonne 0 à 15) |
| `LCDPRINT expr` | affiche nombres/chaînes/concaténations à la position courante |

`LCDPRINT` utilise la même syntaxe que `PRINT` (`;` colle les éléments,
`,` insère un espace) mais **ne revient jamais à la ligne
automatiquement** — contrairement à l'écran série, le curseur du LCD
doit être repositionné explicitement avec `LCDPOS`.

```basic
10 LCDINIT
20 LCDCLR
30 LCDPOS 0,0 : LCDPRINT "BONJOUR"
40 LCDPOS 1,0 : LCDPRINT "TEMP="; T; " C"
```

## 8. Commandes en mode direct (non numérotées)

| Commande | Effet |
|---|---|
| `LIST` | affiche le programme en mémoire |
| `RUN` | réinitialise les variables et exécute le programme depuis le début |
| `NEW` | efface le programme en mémoire |
| `FREE` | affiche l'espace mémoire programme restant |
| `DIR` (ou `FILES`) | liste les fichiers de la carte SD |
| `SAVE nom` | sauvegarde le programme sous `nom.BAS` sur la SD |
| `LOAD nom` | charge `nom.BAS` depuis la SD (remplace le programme courant) |
| `BYE` | quitte BASIC et retourne au bootloader |
| **Ctrl-C** | interrompt un programme en cours d'exécution ou une saisie |

`SAVE`/`LOAD` ajoutent automatiquement l'extension `.BAS` si le nom n'en a
pas déjà une.

## 9. Édition d'un programme

- Taper `numéro instruction` ajoute ou remplace la ligne à ce numéro.
- Taper seulement `numéro` (sans instruction) supprime la ligne.
- Les lignes sont automatiquement classées par ordre croissant de numéro.

```basic
10 PRINT "BONJOUR"
20 PRINT "MONDE"
15 PRINT "---"        ' s'insere entre 10 et 20
20                     ' supprime la ligne 20
LIST
```

## 10. Messages d'erreur

| Message | Cause |
|---|---|
| `SYNTAX` | instruction mal formée |
| `LIGNE INTROUVABLE` | `GOTO`/`GOSUB`/`THEN` vers un numéro qui n'existe pas |
| `MEMOIRE PLEINE` | programme trop volumineux pour la RAM allouée |
| `RETURN SANS GOSUB` | `RETURN` sans `GOSUB` correspondant |
| `NEXT SANS FOR` | `NEXT` sans `FOR` correspondant |
| `DIVISION PAR ZERO` | division ou modulo par 0 |
| `CHAINE TROP LONGUE` | résultat de plus de 40 caractères |
| `FICHIER` | erreur `SAVE`/`LOAD`/`DIR` (carte SD absente, fichier introuvable...) |
| `GOSUB TROP PROFONDS` | plus de 16 `GOSUB` imbriqués |
| `FOR TROP PROFONDS` | plus de 8 `FOR` imbriqués |
| `VALEUR` | argument invalide (ex. `RND(0)` ou négatif) |
| `ARRET` | interruption par Ctrl-C |

## 11. Exemples

### Chenillard sonore sur les LEDs
```basic
10 P=1
20 LED P : DELAY 100 : P=P*2
30 IF P<=$800 THEN 20
40 BELL 880 : DELAY 150 : BELL 0
50 GOTO 10
```

### Table de multiplication
```basic
10 INPUT "TABLE DE";N
20 FOR I=1 TO 10
30 PRINT N;" X ";I;" = ";N*I
40 NEXT I
```

### Devine le nombre
```basic
10 S=RND(100)
20 INPUT "NOMBRE (1-100)";G
30 IF G=S THEN PRINT "GAGNE!" : END
40 IF G<S THEN PRINT "PLUS GRAND"
50 IF G>S THEN PRINT "PLUS PETIT"
60 GOTO 20
```

### Sauvegarde et rechargement
```basic
SAVE JEU
NEW
LOAD JEU
RUN
```

### Compteur sur l'écran LCD
```basic
10 LCDINIT
20 LCDCLR
30 LCDPOS 0,0 : LCDPRINT "COMPTEUR:"
40 N=0
50 LCDPOS 1,0 : LCDPRINT N; "   "
60 N=N+1
70 DELAY 500
80 GOTO 50
```
Les espaces après `N` effacent les anciens chiffres restants quand le
nombre devient plus court (ex. 100 -> 99).

---

## Limites de cette implémentation

- Pas de tableaux, pas de fonctions définies par l'utilisateur (`DEF FN`).
- Pas de `ELSE`, pas d'opérateurs booléens `AND`/`OR`/`NOT`.
- Une seule dimension de chaîne par variable (pas de tableaux de chaînes).
- Programme limité à ~6 Ko (quelques centaines de lignes courantes).
- `RND` a une période limitée (générateur xorshift 32 bits) — largement
  suffisant pour des jeux, pas pour de la cryptographie.
- `LCDPRINT` ne revient jamais à la ligne automatiquement (contrairement à
  `PRINT`) : repositionner le curseur avec `LCDPOS` avant chaque ligne.
