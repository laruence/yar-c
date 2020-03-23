AUTOMAKE_OPTIONS=foreign  # don't check for NEWS ChangeLog etc 
noinst_PROGRAMS=server client
server_SOURCES=server.c
server_LDFLAGS=-levent -lmsgpackc -L$(prefix)/lib/ -Wl,-rpath,$(prefix)/lib/ -lyar
client_SOURCES=client.c
client_LDFLAGS=-levent -lmsgpackc -L$(prefix)/lib/ -Wl,-rpath,$(prefix)/lib/ -lyar
