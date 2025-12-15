#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <signal.h>
#include <limits.h>
#include <libgen.h>
#include <sys/wait.h>

// MOJE PLIKI
#include "pomocnicze.h"
#include "zarzadzanie.h"
#include "proces_potomny.h"
#include "kopia_operacje.h"


volatile sig_atomic_t sygnal_zakoncz = 0;

void wypisz_liste(void);

void handler_sigint(int signo);

void zakoncz_program(void);


void wypisz_liste(void) {
    if (!lista_aktywna) {
        printf("Brak aktywnych kopii.\n");
        return;
    }
    printf("Aktywne kopie:\n");
    for (AktywnyBackup *it = lista_aktywna; it; it = it->nast) {
        printf(" %s -> %s (pid %d)\n", it->sciezka_zrodlowa, it->sciezka_docelowa, it->pid_procesu);
    }
}

void handler_sigint(int signo) {
    (void) signo;
    sygnal_zakoncz = 1;
}

void zakoncz_program(void) {
    AktywnyBackup *it = lista_aktywna;
    while (it) {
        kill(it->pid_procesu, SIGTERM);
        waitpid(it->pid_procesu, NULL, 0);
        it = it->nast;
    }
    while (lista_aktywna) {
        AktywnyBackup *tmp = lista_aktywna;
        lista_aktywna = tmp->nast;
        free(tmp->sciezka_zrodlowa);
        free(tmp->sciezka_docelowa);
        free(tmp);
    }
}


int main(void) {
    struct sigaction sa;
    sa.sa_handler = handler_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("Interaktywny menedzer kopii zapasowych\n");
    printf("Dostepne polecenia: add, end, list, restore, exit\n");

    char *linia = NULL;
    size_t len = 0;

    while (!sygnal_zakoncz) {
        printf("> ");
        fflush(stdout);
        ssize_t r = getline(&linia, &len, stdin);
        if (r <= 0)
            break;
        if (linia[r - 1] == '\n')
            linia[r - 1] = '\0';

        int argc = 0;
        char **argv = parsuj_argumenty(linia, &argc);
        if (argc == 0) {
            zwolnij_argumenty(argv, 0);
            continue;
        }

        if (strcmp(argv[0], "add") == 0) {
            if (argc < 3)
                fprintf(stderr, "Uzycie: add <source> <target1> [target2 ...]\n");
            else
                dodaj_kopie(argv[1], &argv[2], argc - 2);
        } else if (strcmp(argv[0], "end") == 0) {
            if (argc < 3) { fprintf(stderr, "Uzycie: end <source> <target1> [target2 ...]\n"); } else {
                zakoncz_kopie(argv[1], &argv[2], argc - 2);
            }
        } else if (strcmp(argv[0], "list") == 0) {
            wypisz_liste();
        } else if (strcmp(argv[0], "restore") == 0) {
            if (argc != 3) { fprintf(stderr, "Uzycie: restore <source> <target>\n"); } else {
                przywroc_kopie(argv[1], argv[2]);
            }
        } else if (strcmp(argv[0], "exit") == 0) {
            zwolnij_argumenty(argv, argc);
            break;
        } else {
            fprintf(stderr, "Nieznana komenda: %s\n", argv[0]);
        }
        zwolnij_argumenty(argv, argc);
    }
    free(linia);
    zakoncz_program();
    printf("\nKoniec programu.\n");
    return 0;
}
