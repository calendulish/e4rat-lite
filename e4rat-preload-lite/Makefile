# Script de compilação/instalação do e4rat-preload-lite
# Autor: Lara Maia <lara@craft.net.br>
# Versão: 0.2

CFLAGS = -Wall -O2
CC = @gcc
FF = e4rat-preload-lite
FS = e4rat-preload-lite.c

all: main

main:
	@echo 'Compilando objetos...'
	$(CC) -std=c99 $(CFLAGS) -o $(FF) $(FS)
	@strip -s $(FF)
	@echo 'Pronto!'
	@echo 'Execute make install para instalar.'
	
clean:
	@printf 'Limpando...'
	@rm -f $(FF)
	@printf 'Limpo!\n'

install:
	@printf 'Instalando...'
	@cp $(FF) /sbin/
	@printf 'Instalado!\n'

uninstall:
	@printf 'Removendo Arquivos...'
	@rm -f /sbin/$(FF)
	@printf 'Pronto!\n'
