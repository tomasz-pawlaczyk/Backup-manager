#ifndef ZARZADZANIE_H
#define ZARZADZANIE_H

int kopiuj_plik(const char *zrodlo, const char *cel);
int kopiuj_entry(const char *zrodlo_pelna, const char *cel_pelna, const char *sciezka_zrodla_real);
int usun_nadmiarowe(const char *zrodlo, const char *cel);

#endif
