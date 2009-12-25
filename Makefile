all: lf-tests

PKGS = glib-2.0 gthread-2.0 gobject-2.0

WARNINGS =								\
	-Wall -Werror -Wold-style-definition				\
	-Wdeclaration-after-statement					\
	-Wredundant-decls -Wmissing-noreturn -Wshadow -Wcast-align	\
	-Wwrite-strings -Winline -Wformat-nonliteral -Wformat-security	\
	-Wswitch-enum -Wswitch-default -Winit-self			\
	-Wmissing-include-dirs -Wundef -Waggregate-return		\
	-Wmissing-format-attribute -Wnested-externs			\
	$(NULL)

lf_tests_SOURCES = main.c lf-queue.c
lf_tests_HEADERS = lf-queue.h lf-hazard.h

lf-tests: $(lf_tests_SOURCES) $(lf_tests_HEADERS)
	$(CC) -o $@ -g $(WARNINGS) $(lf_tests_SOURCES) \
		`pkg-config --cflags --libs $(PKGS)`

clean:
	rm -rf lf-tests
