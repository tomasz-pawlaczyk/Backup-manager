#ifndef POMOCNICZE_H
#define POMOCNICZE_H

void zwolnij_argumenty(char **args, int n);
int sciezka_jest_wewnatrz(const char *wejsciowa, const char *mozliwy_w_srodku);
int katalog_pusty(const char *sciezka);
char *realpath_safe(const char *p);
int utworz_drzewo_katalogow(const char *sciezka);


char **parsuj_argumenty(const char *linia, int *liczba);



#endif
