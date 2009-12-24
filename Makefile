all: queue-test asyncqueue-test

PKGS = glib-2.0 gthread-2.0

WARNINGS =								\
	-Wall -Werror -Wold-style-definition				\
	-Wdeclaration-after-statement					\
	-Wredundant-decls -Wmissing-noreturn -Wshadow -Wcast-align	\
	-Wwrite-strings -Winline -Wformat-nonliteral -Wformat-security	\
	-Wswitch-enum -Wswitch-default -Winit-self			\
	-Wmissing-include-dirs -Wundef -Waggregate-return		\
	-Wmissing-format-attribute -Wnested-externs			\
	$(NULL)

QUEUE_SOURCES = queue-test.c queue.c queue.h hazard.h

queue-test: $(QUEUE_SOURCES)
	$(CC) -o $@ -g $(WARNINGS) $(QUEUE_SOURCES) \
		`pkg-config --cflags --libs $(PKGS)`

asyncqueue-test: asyncqueue-test.c
	$(CC) -o $@ -g $(WARNINGS) asyncqueue-test.c \
		`pkg-config --cflags --libs $(PKGS)`

clean:
	rm -rf queue-test hlist-test
