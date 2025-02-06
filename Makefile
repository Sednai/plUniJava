MODULE_big = plunijava

OBJS = plunijava.o plunijava_worker.o plunijava_jvm.o plunijava_spi.o

EXTENSION = plunijava
DATA = plunijava--0.0.1.sql
PGFILEDESC = "plUniJava - java language handler"

JAVA_HOME=$(shell readlink -f /usr/bin/javac | sed "s:bin/javac::")

PG_CPPFLAGS += -std=c99 -Wno-error=vla -I$(JAVA_HOME)include -I$(JAVA_HOME)include/linux -fvisibility=default

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
