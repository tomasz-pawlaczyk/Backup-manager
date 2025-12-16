#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>

#include "zarzadzanie.h"
#include "pomocnicze.h"

// W TYM PLIKU UMIESCILEM SREDNIEJ WIELKOSCI FUNKCJE, KTORE ROWNIEZ POMAGAJA W PRACY TYM WIEKSZYM

int kopiuj_plik(const char *zrodlo, const char *cel) {
    int in = open(zrodlo, O_RDONLY);
    if (in < 0)
        return -1;

    int out = open(cel, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        close(in);
        return -1;
    }

    char buf[8192];
    ssize_t r;

    while ((r = read(in, buf, sizeof(buf))) > 0) {
        ssize_t w = write(out, buf, (size_t) r);

        if (w != r) {
            close(in);
            close(out);
            return -1;
        }
    }

    if (r < 0) {
        close(in);
        close(out);
        return -1;
    }

    close(in);
    close(out);

    struct stat st;
    if (stat(zrodlo, &st) == 0) {
        chmod(cel, st.st_mode & 0777);
    }

    return 0;
}

int kopiuj_entry(const char *zrodlo_pelna, const char *cel_pelna, const char *sciezka_zrodla_real) {
    struct stat st;
    if (lstat(zrodlo_pelna, &st) != 0) {
        fprintf(stderr, "Blad lstat %s: %s\n", zrodlo_pelna, strerror(errno));
        return -1;
    }

    if (S_ISLNK(st.st_mode)) {
        char linkbuf[PATH_MAX + 1];
        ssize_t len = readlink(zrodlo_pelna, linkbuf, PATH_MAX);
        if (len < 0)
            return -1;

        linkbuf[len] = '\0';
        if (linkbuf[0] == '/') {
            char *link_real = realpath(linkbuf, NULL);
            if (link_real) {
                if (sciezka_jest_wewnatrz(sciezka_zrodla_real, link_real)) {
                    size_t base_len = strlen(sciezka_zrodla_real);
                    const char *po = link_real + base_len;

                    char nowy_link[PATH_MAX];
                    snprintf(nowy_link, PATH_MAX, "%s%s", cel_pelna, po);

                    unlink(cel_pelna);
                    if (symlink(nowy_link, cel_pelna) != 0) {
                        fprintf(stderr, "Nie udalo sie utworzyc symlink %s -> %s: %s\n", cel_pelna, nowy_link,
                                strerror(errno));
                    }
                } else {
                    unlink(cel_pelna);
                    if (symlink(linkbuf, cel_pelna) != 0) {
                        fprintf(stderr, "Nie udalo sie utworzyc symlink %s -> %s: %s\n", cel_pelna, linkbuf,
                                strerror(errno));
                    }
                }
                free(link_real);
            } else {
                unlink(cel_pelna);
                if (symlink(linkbuf, cel_pelna) != 0) {
                    fprintf(stderr, "Nie udalo sie utworzyc symlink %s -> %s: %s\n", cel_pelna, linkbuf,
                            strerror(errno));
                }
            }
        } else {
            unlink(cel_pelna);
            if (symlink(linkbuf, cel_pelna) != 0) {
                fprintf(stderr, "Nie udalo sie utworzyc symlink %s -> %s: %s\n", cel_pelna, linkbuf, strerror(errno));
            }
        }
        return 0;
    } else if (S_ISDIR(st.st_mode)) {
        if (mkdir(cel_pelna, 0755) != 0) {
            if (errno != EEXIST) {
                fprintf(stderr, "Blad mkdir %s: %s\n", cel_pelna, strerror(errno));
                return -1;
            }
        }
        return 0;
    } else if (S_ISREG(st.st_mode)) {
        struct stat st_dest;
        if (stat(cel_pelna, &st_dest) == 0) {
            if (st_dest.st_mtime >= st.st_mtime)
                return 0;
        }
        if (kopiuj_plik(zrodlo_pelna, cel_pelna) != 0) {
            fprintf(stderr, "Blad kopiowania pliku %s -> %s\n", zrodlo_pelna,
                    cel_pelna);
            return -1;
        }
        return 0;
    } else {
        return 0;
    }
}


int usun_nadmiarowe(const char *zrodlo, const char *cel) {
    DIR *d = opendir(cel);
    if (!d)
        return -1;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char path_cel[PATH_MAX];
        snprintf(path_cel, PATH_MAX, "%s/%s", cel, ent->d_name);

        char path_zrodlo[PATH_MAX];
        snprintf(path_zrodlo, PATH_MAX, "%s/%s", zrodlo, ent->d_name);

        struct stat st_cel;
        if (lstat(path_cel, &st_cel) != 0)
            continue;

        struct stat st_zrodlo;
        if (lstat(path_zrodlo, &st_zrodlo) != 0) {
            if (S_ISDIR(st_cel.st_mode)) {
                DIR *d2 = opendir(path_cel);
                if (d2) {
                    struct dirent *e2;
                    while ((e2 = readdir(d2)) != NULL) {
                        if (strcmp(e2->d_name, ".") == 0 || strcmp(e2->d_name, "..") == 0)
                            continue;

                        char pod[PATH_MAX];
                        int n = snprintf(pod, sizeof(pod), "%s/%s", path_cel, e2->d_name);
                        if (n < 0 || (size_t) n >= sizeof(pod)) {
                            continue;
                        }

                        struct stat stp;
                        if (lstat(pod, &stp) == 0) {
                            if (S_ISDIR(stp.st_mode)) {
                                usun_nadmiarowe("", pod);
                            } else {
                                unlink(pod);
                            }
                        }
                    }
                    closedir(d2);
                }
                rmdir(path_cel);
            } else {
                unlink(path_cel);
            }
        } else {
            if (S_ISDIR(st_cel.st_mode) && S_ISDIR(st_zrodlo.st_mode)) {
                usun_nadmiarowe(path_zrodlo, path_cel);
            }
        }
    }
    closedir(d);
    return 0;
}
