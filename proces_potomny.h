#ifndef WORKER_H
#define WORKER_H
#include  <signal.h>
extern volatile sig_atomic_t sygnal_zakoncz;

int sync_mirror(const char *zrodlo, const char *cel);
int proces_potomny_worker(const char *sciezka_zrodlowa, const char *sciezka_docelowa);

#endif
