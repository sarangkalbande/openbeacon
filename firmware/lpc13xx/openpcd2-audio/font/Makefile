FREQUENCY:=15625

all: clean alphabet.raw
	php create.php

alphabet.raw: alphabet.wav
	sox $? -r $(FREQUENCY) -c1 -b 8 -u $@ silence 1 0.2 2% 1 0.2 2% : newfile : restart

story.raw: story.wav
	sox $? -r $(FREQUENCY) -c1 -b 8 -u $@

play: story.raw
	play -r $(FREQUENCY) -c1 -b 8 -u $?

store: all
	sudo dd_rescue -B 4096 -D database.raw /run/media/$(USER)/OPENBEACON/DATA????.BIN
	sync

clean:
	rm -f alphabet0*.raw [A-Z0-9].png database.raw
