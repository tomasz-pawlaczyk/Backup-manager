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
#include "pomocnicze.h"
#include <stddef.h>

// W TYM PLIKU UMIESCILEM MALE, DODATKOWE FUNCKJE KTORE POMOGAJA W PRACY TYM WIEKSZYM

void zwolnij_argumenty(char **args, int n) {
    if (!args) return;
    for (int i = 0; i < n; ++i)
        free(args[i]);
    free(args);
}


int sciezka_jest_wewnatrz(const char *wejsciowa, const char *mozliwy_w_srodku) {
    size_t l = strlen(wejsciowa);
    if (strncmp(wejsciowa, mozliwy_w_srodku, l) != 0)
        return 0;
    if (strlen(mozliwy_w_srodku) == l)
        return 1;
    return mozliwy_w_srodku[l] == '/';
}


int katalog_pusty(const char *sciezka) {
    DIR *d = opendir(sciezka);
    if (!d) return -1;
    struct dirent *ent;
    int count = 0;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        count++;
        break;
    }
    closedir(d);
    return count == 0;
}


char *realpath_safe(const char *p) {
    char *buf = malloc(PATH_MAX);
    if (!buf) return NULL;
    if (!realpath(p, buf)) {
        free(buf);
        return NULL;
    }
    return buf;
}


int utworz_drzewo_katalogow(const char *sciezka) {
    char tmp[PATH_MAX];
    strncpy(tmp, sciezka, PATH_MAX);
    tmp[PATH_MAX - 1] = '\0';
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0) {
                if (errno != EEXIST) return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0) {
        if (errno != EEXIST) return -1;
    }
    return 0;
}


char **parsuj_argumenty(const char *linia, int *liczba) {
    char *kop = strdup(linia);
    if (!kop)
        return NULL;

    size_t capacity = 8;
    size_t count = 0;
    int error = 0;

    char **res = malloc(sizeof(*res) * capacity);
    if (!res) {
        free(kop);
        return NULL;
    }

    char *p = kop;

    while (*p && !error) {
        while (*p == ' ' || *p == '\t' || *p == '\n')
            p++;

        if (!*p)
            break;

        char *start = NULL;
        char *arg = NULL;

        if (*p == '"') {
            start = ++p;
            while (*p && *p != '"')
                p++;
        } else {
            start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n')
                p++;
        }

        ptrdiff_t diff = p - start;
        if (diff < 0) {
            error = 1;
            break;
        }

        size_t len = (size_t) diff;

        arg = malloc(len + 1);
        if (!arg) {
            error = 1;
            break;
        }

        memcpy(arg, start, len);
        arg[len] = '\0';

        if (*p == '"')
            p++;

        if (count >= capacity) {
            capacity *= 2;
            char **tmp = realloc(res, sizeof(*res) * capacity);
            if (!tmp) {
                free(arg);
                error = 1;
                break;
            }
            res = tmp;
        }

        res[count++] = arg;
    }

    if (error) {
        zwolnij_argumenty(res, (int) count);
        free(kop);
        return NULL;
    }

    char **tmp = realloc(res, sizeof(*res) * (count + 1));
    if (!tmp) {
        zwolnij_argumenty(res, (int) count);
        free(kop);
        return NULL;
    }

    res = tmp;
    res[count] = NULL;
    *liczba = (int) count;

    free(kop);
    return res;
}



