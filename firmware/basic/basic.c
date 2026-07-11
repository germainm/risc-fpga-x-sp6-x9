/* Tiny BASIC pour SoC RISC-V (X-SP6-X9) - ecrit from scratch.
 * Grammaire inspiree de Palo Alto Tiny BASIC, etendue:
 *   - variables numeriques A-Z (int32), chaines A$-Z$ (40 car. max)
 *   - PRINT ? INPUT LET IF/THEN GOTO GOSUB RETURN FOR/NEXT/STEP REM END
 *   - POKE addr,val  PEEK(addr)  litteraux hexa $XXXX
 *   - LED v  BELL hz  SEG d,v  DELAY ms  CLS
 *   - DIP() BTN() INKEY() RND(n) ABS(n) LEN(s$)
 *   - direct: LIST RUN NEW FREE DIR SAVE nom LOAD nom BYE
 *   - ':' separateur d'instructions, Ctrl-C interrompt
 * Toute E/S passe par basio.h (un seul fichier a changer pour VGA etc.)
 */
#include <stdint.h>
#include "basio.h"

#define PROG_SIZE 5120
#define LINE_MAX  128
#define STR_MAX   40
#define GOSUB_MAX 16
#define FOR_MAX   8

/* ---------- etat global ---------- */
static uint8_t prog[PROG_SIZE];      /* [len][num lo][num hi][texte...][0] */
static int prog_end;

static int32_t nvar[26];
static char    svar[26][STR_MAX + 1];

static struct { int line, off; } gstack[GOSUB_MAX];
static int gsp;

typedef struct { int var; int32_t limit, step; int line, off; } forframe;
static forframe fstack[FOR_MAX];
static int fsp;

static const char *tp;               /* pointeur texte courant */
static int cur;                      /* offset ligne courante (-1 = direct) */
static int g_err;                    /* code erreur */
static int ungot = -1;               /* touche relue par INKEY apres ^C-check */
static int out_col;
static uint32_t rnd_state = 0x2545F491;

enum {
    E_NONE = 0, E_SYNTAX, E_NOLINE, E_MEM, E_RETURN, E_NEXT,
    E_DIV0, E_STRLEN, E_FILE, E_GOSUB, E_FOR, E_VALUE, E_BREAK
};
static const char *emsg[] = {
    "", "SYNTAX", "LIGNE INTROUVABLE", "MEMOIRE PLEINE",
    "RETURN SANS GOSUB", "NEXT SANS FOR", "DIVISION PAR ZERO",
    "CHAINE TROP LONGUE", "FICHIER", "GOSUB TROP PROFONDS",
    "FOR TROP PROFONDS", "VALEUR", "ARRET"
};

enum { S_OK, S_NEXTLINE, S_JUMP, S_END, S_ERR, S_BYE };

/* ---------- sortie ---------- */
static void outc(char c)
{
    bas_putc(c);
    if (c == '\n') out_col = 0; else out_col++;
}
static void outs(const char *s) { while (*s) outc(*s++); }
static void outnum(int32_t v)
{
    char b[12]; int i = 0;
    uint32_t u = (v < 0) ? (outc('-'), (uint32_t)(-v)) : (uint32_t)v;
    do { b[i++] = '0' + u % 10; u /= 10; } while (u);
    while (i) outc(b[--i]);
}

/* ---------- clavier ---------- */
static int chk_break(void)
{
    int c = bas_inkey();
    if (c == 3) return 1;
    if (c >= 0 && ungot < 0) ungot = c;
    return 0;
}
static int inkey(void)
{
    if (ungot >= 0) { int c = ungot; ungot = -1; return c; }
    return bas_inkey();
}

static void read_line(char *buf, int max)
{
    int n = 0;
    for (;;) {
        int c = bas_getc();
        if (c == '\r' || c == '\n') { outc('\n'); break; }
        if (c == 3) { n = 0; outs("^C\n"); break; }
        if (c == 8 || c == 127) {
            if (n) { n--; outs("\b \b"); }
            continue;
        }
        if (c >= 32 && c < 127 && n < max - 1) { buf[n++] = (char)c; outc((char)c); }
    }
    buf[n] = 0;
}

/* majuscules hors guillemets */
static void upcase_line(char *s)
{
    int q = 0;
    for (; *s; s++) {
        if (*s == '"') q = !q;
        else if (!q && *s >= 'a' && *s <= 'z') *s -= 32;
    }
}

/* ---------- gestion du programme ---------- */
static int line_num(int off) { return prog[off + 1] | (prog[off + 2] << 8); }
static const char *line_txt(int off) { return (const char *)prog + off + 3; }
static int line_next(int off) { return off + prog[off]; }

static int find_line(int num, int *ins)
{
    int off = 0;
    while (off < prog_end) {
        int n = line_num(off);
        if (n == num) { if (ins) *ins = off; return off; }
        if (n > num) break;
        off = line_next(off);
    }
    if (ins) *ins = off;
    return -1;
}

static int enter_line(int num, const char *text)
{
    int ins, off = find_line(num, &ins);
    if (off >= 0) {                          /* supprimer l'existante */
        int len = prog[off];
        for (int i = off; i < prog_end - len; i++) prog[i] = prog[i + len];
        prog_end -= len;
        if (ins > off) ins -= len;
    }
    int tl = 0; while (text[tl]) tl++;
    if (!tl) return 0;
    int rl = 4 + tl;                         /* len,num2,texte,nul */
    if (rl > 255) return -1;
    if (prog_end + rl > PROG_SIZE) return -1;
    for (int i = prog_end - 1; i >= ins; i--) prog[i + rl] = prog[i];
    prog[ins] = (uint8_t)rl;
    prog[ins + 1] = num & 255; prog[ins + 2] = (num >> 8) & 255;
    for (int i = 0; i <= tl; i++) prog[ins + 3 + i] = (uint8_t)text[i];
    prog_end += rl;
    return 0;
}

/* ---------- analyse lexicale ---------- */
static void skip(void) { while (*tp == ' ' || *tp == '\t') tp++; }

static int kw(const char *k)                 /* mot-cle + frontiere */
{
    const char *p = tp;
    while (*k) { if (*p != *k) return 0; p++; k++; }
    if (*p >= 'A' && *p <= 'Z') return 0;
    if (*p == '$') return 0;
    tp = p;
    return 1;
}

static int is_strvar(void)                   /* lettre suivie de $ */
{
    return tp[0] >= 'A' && tp[0] <= 'Z' && tp[1] == '$';
}

/* ---------- expressions numeriques ---------- */
static int32_t expr(void);
static void seval(char *out);

static int32_t factor(void)
{
    skip();
    if (*tp == '$') {                        /* hexa */
        tp++;
        int32_t v = 0; int n = 0;
        for (;; tp++, n++) {
            char c = *tp;
            if (c >= '0' && c <= '9') v = v * 16 + (c - '0');
            else if (c >= 'A' && c <= 'F') v = v * 16 + (c - 'A' + 10);
            else break;
        }
        if (!n) { g_err = E_SYNTAX; return 0; }
        return v;
    }
    if (*tp >= '0' && *tp <= '9') {
        int32_t v = 0;
        while (*tp >= '0' && *tp <= '9') v = v * 10 + (*tp++ - '0');
        return v;
    }
    if (*tp == '(') {
        tp++;
        int32_t v = expr();
        skip();
        if (*tp != ')') { g_err = E_SYNTAX; return 0; }
        tp++;
        return v;
    }
    if (*tp == '-') { tp++; return -factor(); }

    if (kw("PEEK")) {
        skip(); if (*tp++ != '(') { g_err = E_SYNTAX; return 0; }
        int32_t a = expr();
        skip(); if (*tp++ != ')') { g_err = E_SYNTAX; return 0; }
        return (int32_t)bas_peek((uint32_t)a);
    }
    if (kw("ABS")) {
        skip(); if (*tp++ != '(') { g_err = E_SYNTAX; return 0; }
        int32_t v = expr();
        skip(); if (*tp++ != ')') { g_err = E_SYNTAX; return 0; }
        return v < 0 ? -v : v;
    }
    if (kw("RND")) {
        skip(); if (*tp++ != '(') { g_err = E_SYNTAX; return 0; }
        int32_t n = expr();
        skip(); if (*tp++ != ')') { g_err = E_SYNTAX; return 0; }
        rnd_state ^= rnd_state << 13;
        rnd_state ^= rnd_state >> 17;
        rnd_state ^= rnd_state << 5;
        if (n <= 0) { g_err = E_VALUE; return 0; }
        return (int32_t)(rnd_state % (uint32_t)n);
    }
    if (kw("LEN")) {
        char b[STR_MAX + 1];
        skip(); if (*tp++ != '(') { g_err = E_SYNTAX; return 0; }
        seval(b);
        skip(); if (*tp++ != ')') { g_err = E_SYNTAX; return 0; }
        int l = 0; while (b[l]) l++;
        return l;
    }
    if (kw("DIP"))   { skip(); if (*tp=='('){tp++; skip(); if(*tp++!=')'){g_err=E_SYNTAX;return 0;}} return (int32_t)bas_dip(); }
    if (kw("BTN"))   { skip(); if (*tp=='('){tp++; skip(); if(*tp++!=')'){g_err=E_SYNTAX;return 0;}} return (int32_t)bas_btn(); }
    if (kw("INKEY")) { skip(); if (*tp=='('){tp++; skip(); if(*tp++!=')'){g_err=E_SYNTAX;return 0;}} return inkey(); }

    if (*tp >= 'A' && *tp <= 'Z' && tp[1] != '$')
        return nvar[*tp++ - 'A'];

    g_err = E_SYNTAX;
    return 0;
}

static int32_t term(void)
{
    int32_t v = factor();
    for (;;) {
        skip();
        if (*tp == '*') { tp++; v *= factor(); }
        else if (*tp == '/') {
            tp++;
            int32_t d = factor();
            if (!d) { g_err = E_DIV0; return 0; }
            v /= d;
        } else if (*tp == '%') {
            tp++;
            int32_t d = factor();
            if (!d) { g_err = E_DIV0; return 0; }
            v %= d;
        } else return v;
        if (g_err) return 0;
    }
}

static int32_t expr(void)
{
    int32_t v = term();
    for (;;) {
        skip();
        if (*tp == '+') { tp++; v += term(); }
        else if (*tp == '-') { tp++; v -= term(); }
        else return v;
        if (g_err) return 0;
    }
}

/* ---------- expressions chaines ---------- */
static void scat(char *out, const char *s)
{
    int l = 0; while (out[l]) l++;
    while (*s) {
        if (l >= STR_MAX) { g_err = E_STRLEN; return; }
        out[l++] = *s++;
    }
    out[l] = 0;
}

static void seval(char *out)
{
    out[0] = 0;
    for (;;) {
        skip();
        if (*tp == '"') {
            char b[STR_MAX + 1]; int n = 0;
            tp++;
            while (*tp && *tp != '"') {
                if (n >= STR_MAX) { g_err = E_STRLEN; return; }
                b[n++] = *tp++;
            }
            if (*tp != '"') { g_err = E_SYNTAX; return; }
            tp++;
            b[n] = 0;
            scat(out, b);
        } else if (is_strvar()) {
            scat(out, svar[*tp - 'A']);
            tp += 2;
        } else { g_err = E_SYNTAX; return; }
        if (g_err) return;
        skip();
        if (*tp == '+') { tp++; continue; }
        return;
    }
}

/* ---------- relations ---------- */
static int parse_rel(void)      /* 1:= 2:<> 3:< 4:> 5:<= 6:>= 0:err */
{
    skip();
    if (*tp == '=') { tp++; return 1; }
    if (*tp == '<') {
        tp++;
        if (*tp == '>') { tp++; return 2; }
        if (*tp == '=') { tp++; return 5; }
        return 3;
    }
    if (*tp == '>') {
        tp++;
        if (*tp == '=') { tp++; return 6; }
        return 4;
    }
    return 0;
}

/* ---------- instructions ---------- */
static int stmt(void);

static int do_lcdprint(void)
{
    skip();
    if (!*tp || *tp == ':') return S_OK;
    for (;;) {
        skip();
        if (*tp == '"' || is_strvar()) {
            char b[STR_MAX + 1];
            seval(b);
            if (g_err) return S_ERR;
            bas_lcd_puts(b);
        } else {
            int32_t v = expr();
            if (g_err) return S_ERR;
            char nb[12]; int i = 0;
            uint32_t u = (v < 0) ? (bas_lcd_putc('-'), (uint32_t)(-v)) : (uint32_t)v;
            do { nb[i++] = '0' + u % 10; u /= 10; } while (u);
            while (i) bas_lcd_putc(nb[--i]);
        }
        skip();
        if (*tp == ';') { tp++; skip(); if (!*tp || *tp == ':') return S_OK; continue; }
        if (*tp == ',') { tp++; bas_lcd_putc(' '); skip(); if (!*tp || *tp == ':') return S_OK; continue; }
        return S_OK;             /* pas de retour a la ligne implicite sur LCD */
    }
}

static int print_sexpr(void)      /* stream direct, sans limite 40 car. */
{
    for (;;) {
        skip();
        if (*tp == '"') {
            tp++;
            while (*tp && *tp != '"') outc(*tp++);
            if (*tp != '"') { g_err = E_SYNTAX; return -1; }
            tp++;
        } else if (is_strvar()) {
            outs(svar[*tp - 'A']);
            tp += 2;
        } else { g_err = E_SYNTAX; return -1; }
        skip();
        if (*tp == '+') { tp++; continue; }
        return 0;
    }
}

static int do_print(void)
{
    skip();
    if (!*tp || *tp == ':') { outc('\n'); return S_OK; }
    for (;;) {
        skip();
        if (*tp == '"' || is_strvar()) {
            if (print_sexpr()) return S_ERR;
        } else {
            int32_t v = expr();
            if (g_err) return S_ERR;
            outnum(v);
        }
        skip();
        if (*tp == ';') { tp++; skip(); if (!*tp || *tp == ':') return S_OK; continue; }
        if (*tp == ',') {
            tp++;
            do outc(' '); while (out_col % 8);
            skip();
            if (!*tp || *tp == ':') return S_OK;
            continue;
        }
        outc('\n');
        return S_OK;
    }
}

static int do_input(void)
{
    char buf[LINE_MAX];
    skip();
    if (*tp == '"') {                      /* INPUT "prompt";VAR */
        tp++;
        while (*tp && *tp != '"') outc(*tp++);
        if (*tp != '"') { g_err = E_SYNTAX; return S_ERR; }
        tp++;
        skip();
        if (*tp == ';' || *tp == ',') tp++;
        skip();
    }
    if (is_strvar()) {
        int v = *tp - 'A';
        tp += 2;
        outs("? ");
        read_line(buf, LINE_MAX);
        int i;
        for (i = 0; i < STR_MAX && buf[i]; i++) svar[v][i] = buf[i];
        svar[v][i] = 0;
        return S_OK;
    }
    if (*tp >= 'A' && *tp <= 'Z') {
        int v = *tp++ - 'A';
        for (;;) {
            outs("? ");
            read_line(buf, LINE_MAX);
            upcase_line(buf);
            const char *save = tp;
            tp = buf;
            g_err = 0;
            int32_t val = expr();
            tp = save;
            if (!g_err) { nvar[v] = val; return S_OK; }
            g_err = 0;
            outs("?NOMBRE ATTENDU\n");
        }
    }
    g_err = E_SYNTAX;
    return S_ERR;
}

static int do_goto_line(int num)
{
    int off = find_line(num, 0);
    if (off < 0) { g_err = E_NOLINE; return S_ERR; }
    cur = off;
    return S_JUMP;
}

static int do_list(void)
{
    for (int off = 0; off < prog_end; off = line_next(off)) {
        outnum(line_num(off));
        outc(' ');
        outs(line_txt(off));
        outc('\n');
        if (chk_break()) { g_err = E_BREAK; return S_ERR; }
    }
    return S_OK;
}

static void run_reset(void)
{
    for (int i = 0; i < 26; i++) { nvar[i] = 0; svar[i][0] = 0; }
    gsp = fsp = 0;
}

/* nom de fichier: lettres/chiffres, ajoute .BAS si pas de point */
static int get_fname(char *out)
{
    int n = 0;
    skip();
    if (*tp == '"') tp++;
    while ((*tp >= 'A' && *tp <= 'Z') || (*tp >= '0' && *tp <= '9') ||
           *tp == '.' || *tp == '_' || *tp == '-') {
        if (n < 12) out[n++] = *tp;
        tp++;
    }
    if (*tp == '"') tp++;
    if (!n) return -1;
    out[n] = 0;
    int dot = 0;
    for (int i = 0; i < n; i++) if (out[i] == '.') dot = 1;
    if (!dot && n <= 8) { out[n] = '.'; out[n+1] = 'B'; out[n+2] = 'A'; out[n+3] = 'S'; out[n+4] = 0; }
    return 0;
}

static int do_save(void)
{
    char name[16];
    if (get_fname(name)) { g_err = E_SYNTAX; return S_ERR; }
    if (fio_open_write(name)) { g_err = E_FILE; return S_ERR; }
    for (int off = 0; off < prog_end; off = line_next(off)) {
        char b[LINE_MAX + 8]; int n = 0;
        int num = line_num(off);
        char d[6]; int i = 0;
        do { d[i++] = '0' + num % 10; num /= 10; } while (num);
        while (i) b[n++] = d[--i];
        b[n++] = ' ';
        const char *t = line_txt(off);
        while (*t) b[n++] = *t++;
        b[n++] = '\r'; b[n++] = '\n';
        if (fio_write(b, n)) { fio_close(); g_err = E_FILE; return S_ERR; }
    }
    fio_close();
    outs("SAUVE: "); outs(name); outc('\n');
    return S_OK;
}

static int do_load(void)
{
    char name[16], lb[LINE_MAX];
    if (get_fname(name)) { g_err = E_SYNTAX; return S_ERR; }
    if (fio_open_read(name)) { g_err = E_FILE; return S_ERR; }
    prog_end = 0;
    int n = 0, eof = 0;
    while (!eof) {
        char c;
        int r = fio_read(&c, 1);
        if (r < 0) { fio_close(); g_err = E_FILE; return S_ERR; }
        if (r == 0) { eof = 1; c = '\n'; }
        if (c == '\r') continue;
        if (c != '\n') {
            if (n < LINE_MAX - 1) lb[n++] = c;
            continue;
        }
        lb[n] = 0;
        n = 0;
        upcase_line(lb);
        const char *p = lb;
        while (*p == ' ') p++;
        if (!*p) continue;
        int num = 0, d = 0;
        while (*p >= '0' && *p <= '9') { num = num * 10 + (*p++ - '0'); d = 1; }
        while (*p == ' ') p++;
        if (!d) { fio_close(); g_err = E_FILE; return S_ERR; }
        if (enter_line(num, p)) { fio_close(); g_err = E_MEM; return S_ERR; }
    }
    fio_close();
    outs("CHARGE: "); outs(name); outc('\n');
    return S_OK;
}

static int do_dir(void)
{
    char name[13];
    uint32_t size;
    if (fio_dir_open()) { g_err = E_FILE; return S_ERR; }
    while (fio_dir_next(name, &size) == 0) {
        outs(name);
        int l = 0; while (name[l]) l++;
        while (l++ < 14) outc(' ');
        outnum((int32_t)size);
        outc('\n');
    }
    return S_OK;
}

static int stmt(void)
{
    skip();
    if (!*tp || *tp == ':') return S_OK;

    if (kw("REM")) return S_NEXTLINE;
    if (kw("PRINT") || (*tp == '?' && (tp++, 1))) return do_print();
    if (kw("INPUT")) return do_input();
    if (kw("GOTO")) {
        int32_t n = expr();
        if (g_err) return S_ERR;
        return do_goto_line((int)n);
    }
    if (kw("GOSUB")) {
        int32_t n = expr();
        if (g_err) return S_ERR;
        if (gsp >= GOSUB_MAX) { g_err = E_GOSUB; return S_ERR; }
        gstack[gsp].line = cur;
        gstack[gsp].off = (int)(tp - (const char *)prog);
        gsp++;
        return do_goto_line((int)n);
    }
    if (kw("RETURN")) {
        if (!gsp) { g_err = E_RETURN; return S_ERR; }
        gsp--;
        cur = gstack[gsp].line;
        if (cur < 0) return S_END;   /* GOSUB venait du mode direct */
        tp = (const char *)prog + gstack[gsp].off;
        return S_OK;
    }
    if (kw("FOR")) {
        skip();
        if (!(*tp >= 'A' && *tp <= 'Z') || tp[1] == '$') { g_err = E_SYNTAX; return S_ERR; }
        int v = *tp++ - 'A';
        skip();
        if (*tp++ != '=') { g_err = E_SYNTAX; return S_ERR; }
        int32_t a = expr();
        if (g_err) return S_ERR;
        skip();
        if (!kw("TO")) { g_err = E_SYNTAX; return S_ERR; }
        int32_t b = expr();
        if (g_err) return S_ERR;
        int32_t st = 1;
        skip();
        if (kw("STEP")) {
            st = expr();
            if (g_err) return S_ERR;
        }
        nvar[v] = a;
        if (fsp >= FOR_MAX) { g_err = E_FOR; return S_ERR; }
        fstack[fsp].var = v;
        fstack[fsp].limit = b;
        fstack[fsp].step = st;
        fstack[fsp].line = cur;
        fstack[fsp].off = (int)(tp - (const char *)prog);
        fsp++;
        return S_OK;
    }
    if (kw("NEXT")) {
        skip();
        int v = -1;
        if (*tp >= 'A' && *tp <= 'Z' && tp[1] != '$') v = *tp++ - 'A';
        while (fsp && v >= 0 && fstack[fsp - 1].var != v) fsp--;
        if (!fsp) { g_err = E_NEXT; return S_ERR; }
        forframe *f = &fstack[fsp - 1];
        nvar[f->var] += f->step;
        int done = (f->step >= 0) ? (nvar[f->var] > f->limit)
                                  : (nvar[f->var] < f->limit);
        if (done) { fsp--; return S_OK; }
        cur = f->line;
        tp = (const char *)prog + f->off;
        return S_OK;
    }
    if (kw("IF")) {
        int truth;
        skip();
        if (*tp == '"' || is_strvar()) {
            char a[STR_MAX + 1], b[STR_MAX + 1];
            seval(a);
            if (g_err) return S_ERR;
            int rel = parse_rel();
            if (rel != 1 && rel != 2) { g_err = E_SYNTAX; return S_ERR; }
            seval(b);
            if (g_err) return S_ERR;
            int c = 0, i = 0;
            for (;; i++) {
                if (a[i] != b[i]) { c = 1; break; }
                if (!a[i]) break;
            }
            truth = (rel == 1) ? !c : c;
        } else {
            int32_t a = expr();
            if (g_err) return S_ERR;
            int rel = parse_rel();
            if (!rel) { g_err = E_SYNTAX; return S_ERR; }
            int32_t b = expr();
            if (g_err) return S_ERR;
            switch (rel) {
            case 1: truth = a == b; break;
            case 2: truth = a != b; break;
            case 3: truth = a < b;  break;
            case 4: truth = a > b;  break;
            case 5: truth = a <= b; break;
            default: truth = a >= b; break;
            }
        }
        skip();
        if (!kw("THEN")) { g_err = E_SYNTAX; return S_ERR; }
        if (!truth) return S_NEXTLINE;
        skip();
        if (*tp >= '0' && *tp <= '9') {
            int32_t n = expr();
            if (g_err) return S_ERR;
            return do_goto_line((int)n);
        }
        return stmt();
    }
    if (kw("POKE")) {
        int32_t a = expr();
        if (g_err) return S_ERR;
        skip();
        if (*tp++ != ',') { g_err = E_SYNTAX; return S_ERR; }
        int32_t v = expr();
        if (g_err) return S_ERR;
        bas_poke((uint32_t)a, (uint32_t)v);
        return S_OK;
    }
    if (kw("LED")) {
        int32_t v = expr();
        if (g_err) return S_ERR;
        bas_led((uint32_t)v);
        return S_OK;
    }
    if (kw("BELL")) {
        int32_t v = expr();
        if (g_err) return S_ERR;
        bas_bell((uint32_t)v);
        return S_OK;
    }
    if (kw("SEG")) {
        int32_t d = expr();
        if (g_err) return S_ERR;
        skip();
        if (*tp++ != ',') { g_err = E_SYNTAX; return S_ERR; }
        int32_t v = expr();
        if (g_err) return S_ERR;
        bas_seg((int)d, (int)v);
        return S_OK;
    }
    if (kw("DELAY")) {
        int32_t ms = expr();
        if (g_err) return S_ERR;
        while (ms > 0) {
            bas_delay_ms(ms > 10 ? 10 : (uint32_t)ms);
            ms -= 10;
            if (chk_break()) { g_err = E_BREAK; return S_ERR; }
        }
        return S_OK;
    }
    if (kw("CLS")) { outs("\033[2J\033[H"); out_col = 0; return S_OK; }
    if (kw("LCDINIT"))  { bas_lcd_init(); return S_OK; }
    if (kw("LCDCLR"))   { bas_lcd_clear(); return S_OK; }
    if (kw("LCDPOS")) {
        int32_t r = expr();
        if (g_err) return S_ERR;
        skip();
        if (*tp++ != ',') { g_err = E_SYNTAX; return S_ERR; }
        int32_t c = expr();
        if (g_err) return S_ERR;
        bas_lcd_goto((int)r, (int)c);
        return S_OK;
    }
    if (kw("LCDPRINT")) return do_lcdprint();
    if (kw("END") || kw("STOP")) return S_END;
    if (kw("LIST")) return do_list();
    if (kw("RUN")) {
        run_reset();
        if (!prog_end) return S_END;
        cur = 0;
        return S_JUMP;
    }
    if (kw("NEW")) { prog_end = 0; run_reset(); return S_END; }
    if (kw("FREE")) {
        outnum(PROG_SIZE - prog_end);
        outs(" OCTETS LIBRES\n");
        return S_OK;
    }
    if (kw("SAVE")) return do_save();
    if (kw("LOAD")) return do_load();
    if (kw("DIR") || kw("FILES")) return do_dir();
    if (kw("BYE")) return S_BYE;
    if (kw("LET")) skip();

    /* affectation */
    if (is_strvar()) {
        int v = *tp - 'A';
        tp += 2;
        skip();
        if (*tp++ != '=') { g_err = E_SYNTAX; return S_ERR; }
        char b[STR_MAX + 1];
        seval(b);
        if (g_err) return S_ERR;
        for (int i = 0; i <= STR_MAX; i++) svar[v][i] = b[i];
        return S_OK;
    }
    if (*tp >= 'A' && *tp <= 'Z') {
        int v = *tp++ - 'A';
        skip();
        if (*tp++ != '=') { g_err = E_SYNTAX; return S_ERR; }
        int32_t val = expr();
        if (g_err) return S_ERR;
        nvar[v] = val;
        return S_OK;
    }
    g_err = E_SYNTAX;
    return S_ERR;
}

/* ---------- boucle d'execution d'un programme/ligne ---------- */
static void report(void)
{
    outs("?ERREUR ");
    outs(emsg[g_err]);
    if (cur >= 0) { outs(" EN "); outnum(line_num(cur)); }
    outc('\n');
    g_err = 0;
}

static int exec_from(int start_off)   /* retourne S_BYE ou S_END */
{
    cur = start_off;
    tp = line_txt(cur);
    for (;;) {
        if (chk_break()) { g_err = E_BREAK; report(); return S_END; }
        skip();
        if (*tp == ':') { tp++; continue; }
        if (!*tp) {
            cur = line_next(cur);
            if (cur >= prog_end) return S_END;
            tp = line_txt(cur);
            continue;
        }
        int st = stmt();
        switch (st) {
        case S_OK: break;
        case S_NEXTLINE:
            cur = line_next(cur);
            if (cur >= prog_end) return S_END;
            tp = line_txt(cur);
            break;
        case S_JUMP:
            tp = line_txt(cur);
            break;
        case S_END: return S_END;
        case S_BYE: return S_BYE;
        default: report(); return S_END;
        }
    }
}

void main(void)
{
    char buf[LINE_MAX];

    rnd_state = bas_ticks() | 1;
    outs("\n*** TINY BASIC RISC-V v1.0 ***\n");
    outnum(PROG_SIZE); outs(" OCTETS PROGRAMME\n");
    outs("PRET\n");

    for (;;) {
        outs("> ");
        read_line(buf, LINE_MAX);
        upcase_line(buf);
        const char *p = buf;
        while (*p == ' ') p++;
        if (!*p) continue;

        if (*p >= '0' && *p <= '9') {        /* edition de ligne */
            int num = 0;
            while (*p >= '0' && *p <= '9') num = num * 10 + (*p++ - '0');
            while (*p == ' ') p++;
            if (enter_line(num, p)) { g_err = E_MEM; cur = -1; report(); }
            continue;
        }

        cur = -1;
        tp = p;
        g_err = 0;
        for (;;) {
            skip();
            if (*tp == ':') { tp++; continue; }
            if (!*tp) break;
            int st = stmt();
            if (st == S_ERR) { report(); break; }
            if (st == S_BYE) { outs("AU REVOIR\n"); return; }
            if (st == S_JUMP) {              /* RUN ou GOTO direct */
                if (exec_from(cur) == S_BYE) { outs("AU REVOIR\n"); return; }
                outs("PRET\n");
                break;
            }
            if (st != S_OK) break;           /* END, REM, IF faux */
        }
    }
}
