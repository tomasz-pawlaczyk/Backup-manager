#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <signal.h>
#include <limits.h>

#include "proces_potomny.h"
#include "pomocnicze.h"
#include "zarzadzanie.h"

// W TYM PLIKU ZNAJDUJE SIE IMPLEMENTACJA DZIALANIA PROCESU POTOMNEGO I SYNC_MIRROR

int sync_mirror(const char *zrodlo, const char *cel) {
    if (utworz_drzewo_katalogow(cel) != 0) {
        fprintf(stderr, "Nie mozna utworzyc katalogu docelowego %s\n", cel);
        return -1;
    }

    struct stack_item {
        char z[PATH_MAX];
        char c[PATH_MAX];
    };
    struct stack_item *stack = malloc(sizeof(struct stack_item) * 1024);

    size_t sp = 0;
    strncpy(stack[sp].z, zrodlo, PATH_MAX);
    strncpy(stack[sp].c, cel, PATH_MAX);
    sp++;
    char *sciezka_zrodla_real = realpath_safe(zrodlo);

    if (!sciezka_zrodla_real)
        sciezka_zrodla_real = strdup(zrodlo);

    while (sp > 0) {
        struct stack_item si = stack[--sp];
        DIR *d = opendir(si.z);
        if (!d) continue;
        struct dirent *ent;

        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            char path_z[PATH_MAX];
            char path_c[PATH_MAX];

            int n1 = snprintf(path_z, sizeof(path_z), "%s/%s", si.z, ent->d_name);
            if (n1 < 0 || (size_t) n1 >= sizeof(path_z)) {
                continue;
            }

            int n2 = snprintf(path_c, PATH_MAX, "%s/%s", si.c, ent->d_name);
            if (n2 < 0 || (size_t) n2 >= sizeof(path_c)) {
                continue;
            }

            struct stat st;
            if (lstat(path_z, &st) != 0)
                continue;
            if (S_ISDIR(st.st_mode)) {
                if (mkdir(path_c, 0755) != 0) {
                    if (errno != EEXIST) {
                        fprintf(stderr, "Blad mkdir %s: %s\n", path_c, strerror(errno));
                    }
                }

                if (sp % 1024 == 1023) {
                    stack = realloc(stack, sizeof(struct stack_item) * (sp + 1024));
                }
                strncpy(stack[sp].z, path_z, PATH_MAX);
                strncpy(stack[sp].c, path_c, PATH_MAX);
                sp++;
            } else {
                kopiuj_entry(path_z, path_c, sciezka_zrodla_real);
            }
        }
        closedir(d);
    }
    free(stack);
    free(sciezka_zrodla_real);

    usun_nadmiarowe(zrodlo, cel);
    return 0;
}


int proces_potomny_worker(const char *sciezka_zrodlowa, const char *sciezka_docelowa) {
    if (sync_mirror(sciezka_zrodlowa, sciezka_docelowa) != 0) {
        fprintf(stderr, "Blad podczas poczatkowej synchronizacji %s -> %s\n", sciezka_zrodlowa, sciezka_docelowa);
    }

    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "inotify_init error: %s\n", strerror(errno));

        while (!sygnal_zakoncz)
            sleep(1);
        return 0;
    }

    struct watch_item {
        int wd;
        char sc[PATH_MAX];
        struct watch_item *next;
    }
            *watch_head = NULL;

    struct stack_item2 {
        char pz[PATH_MAX];
    };
    struct stack_item2 *st = malloc(sizeof(struct stack_item2) * 1024);

    size_t sp = 0;
    strncpy(st[sp].pz, sciezka_zrodlowa, PATH_MAX);
    sp++;

    while (sp > 0) {
        struct stack_item2 cur = st[--sp];
        int wd = inotify_add_watch(fd, cur.pz, IN_CREATE | IN_DELETE |
                                               IN_MODIFY | IN_ATTRIB | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF |
                                               IN_MOVE_SELF);
        if (wd < 0) {
            strerror(errno);
        } else {
            struct watch_item *it = malloc(sizeof(struct watch_item));
            it->wd = wd;
            strncpy(it->sc, cur.pz, PATH_MAX);
            it->next = watch_head;
            watch_head = it;
        }
        DIR *d = opendir(cur.pz);
        if (!d) continue;
        struct dirent *ent;

        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;

            char path[PATH_MAX];
            int n = snprintf(path, sizeof(path), "%s/%s", cur.pz, ent->d_name);
            if (n < 0 || (size_t) n >= sizeof(path)) {
                continue;
            }

            struct stat stt;
            if (lstat(path, &stt) == 0 && S_ISDIR(stt.st_mode)) {
                if (sp % 1024 == 1023)
                    st = realloc(st, sizeof(struct stack_item2) * (sp + 1024));
                strncpy(st[sp].pz, path, PATH_MAX);
                sp++;
            }
        }
        closedir(d);
    }
    free(st);

    char buf[4096]
            __attribute__ ((aligned(__alignof__(struct inotify_event))));
    while (!sygnal_zakoncz) {
        ssize_t len = read(fd, buf, sizeof(buf));
        if (len <= 0) {
            sleep(1);
            continue;
        }
        if (sync_mirror(sciezka_zrodlowa, sciezka_docelowa) != 0) {
            fprintf(stderr, "Blad synchronizacji po zdarzeniu: %s -> %s\n", sciezka_zrodlowa, sciezka_docelowa);
        }
    }

    struct watch_item *it = watch_head;
    while (it) {
        inotify_rm_watch(fd, it->wd);
        struct watch_item *nx = it->next;
        free(it);
        it = nx;
    }
    close(fd);
    return 0;
}
