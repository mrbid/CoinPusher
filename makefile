all:
	gcc main.c glad_gl.c -I inc -Ofast -lglfw -lm -o coinpusher

install:
	cp coinpusher $(DESTDIR)

uninstall:
	rm $(DESTDIR)/coinpusher

clean:
	rm coinpusher