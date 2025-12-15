#ifndef KOPIA_OPERACJE_H
#define KOPIA_OPERACJE_H

#include <signal.h>
#include <sys/types.h>

typedef struct AktywnyBackup {
    char *sciezka_zrodlowa;
    char *sciezka_docelowa;
    pid_t pid_procesu;
    struct AktywnyBackup *nast;
} AktywnyBackup;

extern AktywnyBackup *lista_aktywna;

int dodaj_kopie(const char *sciezka_zrodlowa_raw, char **sciezki_docelowe, int ile_docelowych);

int zakoncz_kopie(const char *sciezka_zrodlowa_raw, char **sciezki_docelowe, int ile_docelowych);

int przywroc_kopie(const char *sciezka_zrodlowa_raw, const char *sciezka_docelowa_raw);

extern AktywnyBackup *lista_aktywna;

#endif
