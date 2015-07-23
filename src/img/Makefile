
L_PNGS = $(shell cat labirinth.xml | grep frame | grep -v 'robot/' | sed 's/^.*img="//g; s/".*$$//g')
R_PNGS = $(shell cat labirinth.xml | grep frame | grep 'robot/' | sed 's/^.*img="//g; s/".*$$//g')

all: $(L_PNGS) $(R_PNGS)
	@echo Done

L_POVS = $(patsubst %.png,%.pov,$(L_PNGS))

$(R_PNGS): robot.py robot.pov
	python robot.py

%.png: %.pov
	povray -d +H250 +W250 $<

clean:
	rm -f $(L_PNGS) $(R_PNGS)

.PHONY: all clean

