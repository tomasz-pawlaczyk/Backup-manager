#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include "kopia_operacje.h"
#include "pomocnicze.h"
#include "proces_potomny.h"
#include "zarzadzanie.h"

AktywnyBackup *lista_aktywna = NULL;

// FUNKCJE, KTORE DOTYCZA KOPII

int dodaj_kopie(const char *sciezka_zrodlowa_raw, char **sciezki_docelowe, int ile_docelowych) {
    char *sciezka_zrodlowa_real = realpath_safe(sciezka_zrodlowa_raw);
    if (!sciezka_zrodlowa_real) {
        fprintf(stderr, "Zrodlo nie istnieje lub nie mozna uzyskac realpath: %s\n", sciezka_zrodlowa_raw);
        return -1;
    }
    for (int i = 0; i < ile_docelowych; ++i) {
        const char *doc_raw = sciezki_docelowe[i];

        if (access(doc_raw, F_OK) != 0) {
            if (utworz_drzewo_katalogow(doc_raw) != 0) {
                fprintf(stderr, "Nie mozna utworzyc katalogu docelowego %s\n", doc_raw);
                continue;
            }
        }
        char *doc_real = realpath_safe(doc_raw);
        if (!doc_real) {
            fprintf(stderr, "Nie mozna uzyskac realpath dla docelowego %s\n", doc_raw);
            continue;
        }

        if (sciezka_jest_wewnatrz(sciezka_zrodlowa_real, doc_real)) {
            fprintf(stderr, "Blad: nie mozna tworzyc kopii wewnatrz katalogu zrodlowego: %s\n", doc_real);
            free(doc_real);
            continue;
        }

        int duplikat = 0;
        for (AktywnyBackup *it = lista_aktywna; it; it = it->nast) {
            if (strcmp(it->sciezka_zrodlowa, sciezka_zrodlowa_real) == 0 && strcmp(it->sciezka_docelowa, doc_real) ==
                0) {
                duplikat = 1;
                break;
            }
        }
        if (duplikat) {
            fprintf(stderr, "Para zrodlo/docelowy juz istnieje: %s -> %s\n", sciezka_zrodlowa_real, doc_real);
            free(doc_real);
            continue;
        }

        int pusty = katalog_pusty(doc_real);
        if (pusty == 0) {
            fprintf(stderr, "Katalog docelowy musi byc pusty: %s\n", doc_real);
            free(doc_real);
            continue;
        } else if (pusty < 0) {
            fprintf(stderr, "Nie mozna sprawdzic katalogu docelowego: %s\n", doc_real);
            free(doc_real);
            continue;
        }
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Fork nie powiodl sie\n");
            free(doc_real);
            continue;
        } else if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGTERM, SIG_DFL);

            proces_potomny_worker(sciezka_zrodlowa_real, doc_real);
            _exit(0);
        } else {
            AktywnyBackup *n = malloc(sizeof(AktywnyBackup));
            n->sciezka_zrodlowa = strdup(sciezka_zrodlowa_real);
            n->sciezka_docelowa = strdup(doc_real);
            n->pid_procesu = pid;
            n->nast = lista_aktywna;
            lista_aktywna = n;
            fprintf(stderr, "Utworzono kopie: %s -> %s (pid %d)\n", sciezka_zrodlowa_real, doc_real, pid);
            free(doc_real);
        }
    }
    free(sciezka_zrodlowa_real);
    return 0;
}


int zakoncz_kopie(const char *sciezka_zrodlowa_raw, char **sciezki_docelowe, int ile_docelowych) {
    char *sciezka_zrodlowa_real = realpath_safe(sciezka_zrodlowa_raw);
    if (!sciezka_zrodlowa_real) {
        fprintf(stderr, "Nie mozna uzyskac realpath dla zrodla: %s\n", sciezka_zrodlowa_raw);
        return -1;
    }
    for (int i = 0; i < ile_docelowych; ++i) {
        char *doc_real = realpath_safe(sciezki_docelowe[i]);
        if (!doc_real) {
            fprintf(stderr, "Nie mozna uzyskac realpath dla docelowego: %s\n", sciezki_docelowe[i]);
            continue;
        }
        AktywnyBackup *prev = NULL;
        AktywnyBackup *it = lista_aktywna;
        while (it) {
            if (strcmp(it->sciezka_zrodlowa, sciezka_zrodlowa_real) == 0 && strcmp(it->sciezka_docelowa, doc_real) ==
                0) {
                kill(it->pid_procesu, SIGTERM);
                waitpid(it->pid_procesu, NULL, 0);
                fprintf(stderr, "Zakonczono kopie: %s -> %s (pid %d)\n", it->sciezka_zrodlowa, it->sciezka_docelowa,
                        it->pid_procesu);

                if (prev)
                    prev->nast = it->nast;
                else
                    lista_aktywna = it->nast;

                free(it->sciezka_zrodlowa);
                free(it->sciezka_docelowa);
                AktywnyBackup *tmp = it;
                it = it->nast;
                free(tmp);
                continue;
            }
            prev = it;
            it = it->nast;
        }
        free(doc_real);
    }
    free(sciezka_zrodlowa_real);
    return 0;
}


int przywroc_kopie(const char *sciezka_zrodlowa_raw, const char *sciezka_docelowa_raw) {
    char *src_real = realpath_safe(sciezka_zrodlowa_raw);
    if (!src_real) {
        if (utworz_drzewo_katalogow(sciezka_zrodlowa_raw) != 0) {
            fprintf(stderr, "Nie mozna utworzyc katalogu zrodla: %s\n", sciezka_zrodlowa_raw);
            return -1;
        }
        src_real = realpath_safe(sciezka_zrodlowa_raw);
        if (!src_real) {
            fprintf(stderr, "Brak realpath po utworzeniu zrodla: %s\n", sciezka_zrodlowa_raw);
            return -1;
        }
    }
    char *dst_real = realpath_safe(sciezka_docelowa_raw);
    if (!dst_real) {
        fprintf(stderr, "Nie mozna uzyskac realpath docelowego: %s\n", sciezka_docelowa_raw);
        free(src_real);
        return -1;
    }

    if (sciezka_jest_wewnatrz(src_real, dst_real)) {
        fprintf(stderr, "Blad: katalog docelowy przywracania nie moze byc wewnatrz katalogu zrodlowego\n");
        free(src_real);
        free(dst_real);
        return -1;
    }

    if (sync_mirror(dst_real, src_real) != 0) {
        fprintf(stderr, "Blad przywracania %s -> %s\n", dst_real, src_real);
    } else {
        fprintf(stderr, "Przywrocono kopie: %s -> %s\n", dst_real, src_real);
    }
    free(src_real);
    free(dst_real);
    return 0;
}
