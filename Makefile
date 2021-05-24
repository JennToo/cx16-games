GAMES := weiqi16 crosswind

.PHONY: all
all: $(GAMES:%=build/%.prg)

WEIQI16_SRCS := games/weiqi16/main.c
build/weiqi16.prg: $(WEIQI16_SRCS) build
	cl65 -t cx16 -O -o $@ $(WEIQI16_SRCS)

CROSSWIND_SRCS := games/crosswind/main.a65
build/crosswind.prg: $(CROSSWIND_SRCS) build
	cl65 -t cx16 -o $@ $(CROSSWIND_SRCS)

RUN_TARGETS := $(addprefix run-,$(GAMES))
$(RUN_TARGETS): run-%: build/%.prg
	./scripts/run $<

build:
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf build
