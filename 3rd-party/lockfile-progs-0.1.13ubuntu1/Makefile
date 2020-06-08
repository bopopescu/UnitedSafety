
LOADLIBES := -llockfile
CFLAGS    := -g -Wall -Wformat-security -Werror -O2

all: lockfile-create
	rm -rf bin
	mkdir -p bin
	cp -a lockfile-create bin
	cp -a lockfile-create bin/mail-lock
	cd bin && ln lockfile-create lockfile-remove
	cd bin && ln lockfile-create lockfile-touch
	cd bin && ln lockfile-create lockfile-check
	cd bin && ln mail-lock mail-unlock
	cd bin && ln mail-lock mail-touchlock
	mkdir -p man
	cp -a lockfile-progs.1 man
	(cd man && ln -sf lockfile-progs.1 lockfile-create.1 && \
	           ln -sf lockfile-progs.1 lockfile-remove.1 && \
	           ln -sf lockfile-progs.1 lockfile-touch.1 && \
	           ln -sf lockfile-progs.1 lockfile-check.1 && \
	           ln -sf lockfile-progs.1 mail-lock.1 && \
	           ln -sf lockfile-progs.1 mail-unlock.1 && \
	           ln -sf lockfile-progs.1 mail-touchlock.1)
.PHONY: all

lockfile-create: lockfile-progs.o
	${CC} -o $@ ${LDFLAGS} $^ ${LOADLIBES}

# These tests are quite insufficient, but perhaps better than nothing for now.
check: all
	rm -rf check
	mkdir check

	bin/lockfile-create check/file
	bin/lockfile-touch --oneshot check/file
	bin/lockfile-check check/file
	bin/lockfile-remove check/file
	! test -e check/file.lock

	bin/lockfile-create --lock-name check/file.lock
	bin/lockfile-touch --oneshot --lock-name check/file.lock
	bin/lockfile-check --lock-name check/file.lock
	bin/lockfile-remove --lock-name check/file.lock
	! test -e check/file.lock

	bin/lockfile-create --use-pid --lock-name check/file.lock
	bin/lockfile-touch --oneshot --lock-name check/file.lock
        # PID shouldn't be the same, so this should fail.
	! bin/lockfile-check --use-pid --lock-name check/file.lock
	bin/lockfile-remove --lock-name check/file.lock
	! test -e check/file.lock

	bin/lockfile-create --use-pid --lock-name check/lockfile.no-pid
.PHONY: check

distclean: clean
clean:
	rm -f lockfile-create lockfile-remove lockfile-touch lockfile-check
	rm -f mail-lock mail-unlock mail-touchlock
	rm -f *.o *~
	rm -rf bin man
	rm -rf check
.PHONY: clean distclean
