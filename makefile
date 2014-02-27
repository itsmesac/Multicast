all: host router 

generic.o: generic.cpp
	g++ -c generic.cpp

host: host.cpp
	g++ -o host host.cpp

router.o: router.cpp
	g++ -c router.cpp

router: router.o generic.o
	g++ -o router router.o generic.o
