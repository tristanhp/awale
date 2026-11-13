# Makefile du serveur de jeu Awale
#   make              compile le serveur et le client (bin/)
#   make debug        meme chose avec -g et les detecteurs d'erreurs memoire (bin-debug/)
#   make test         lance les tests du moteur, du protocole et des parties
#   make test-debug   idem, mais programmes compiles en mode debug (ASan/UBSan)
#   make clean        supprime tout ce qui est genere

CC = gcc
BUILD ?= release

ifeq ($(BUILD),debug)
CFLAGS  = -Wall -Wextra -std=gnu99 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS = -fsanitize=address,undefined
OBJDIR  = obj-debug
BINDIR  = bin-debug
else
CFLAGS  = -Wall -Wextra -std=gnu99 -O2
LDFLAGS =
OBJDIR  = obj
BINDIR  = bin
endif

# -MMD -MP : genere les dependances sur les .h (un .h modifie recompile les .c concernes)
CFLAGS += -MMD -MP

SERVEUR_OBJS = $(addprefix $(OBJDIR)/, server.o clients.o protocole.o commandes.o parties.o tampon.o awale.o)
CLIENT_OBJS  = $(addprefix $(OBJDIR)/, client.o tampon.o awale.o)

all: $(BINDIR)/awale_serveur $(BINDIR)/awale_client

$(BINDIR)/awale_serveur: $(SERVEUR_OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

$(BINDIR)/awale_client: $(CLIENT_OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR) $(BINDIR) tests/bin:
	mkdir -p $@

debug:
	$(MAKE) BUILD=debug all

# tests unitaires du moteur (aucun reseau) + scenarios du protocole (serveur reel + nc)
tests/bin/test_awale: tests/test_awale.c src/awale.c src/awale.h | tests/bin
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_awale.c src/awale.c

test: all tests/bin/test_awale
	tests/bin/test_awale
	BIN=$(BINDIR) tests/test_proto.sh
	BIN=$(BINDIR) tests/test_parties.sh
	BIN=$(BINDIR) bash tests/test_elo.sh
	BIN=$(BINDIR) bash tests/test_chat.sh

test-debug:
	$(MAKE) BUILD=debug test

clean:
	rm -rf obj obj-debug bin bin-debug tests/bin

.PHONY: all debug test test-debug clean

-include $(wildcard $(OBJDIR)/*.d)
