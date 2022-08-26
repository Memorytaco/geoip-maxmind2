all: libmaxmind
	./compile

libmaxmind:
	cd libmaxminddb && ./configure && make && cd -
	cp libmaxminddb/src/.libs/libmaxminddb.a .
