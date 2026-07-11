#ifndef EEPROM_H
#define EEPROM_H
/* 24C04: memorisation du dernier payload (offset 0, page 0) */
int eeprom_get_last(char *name13);           /* 0 = ok, nom valide copie */
int eeprom_set_last(const char *name);       /* 0 = ok (n'ecrit que si change) */
#endif
