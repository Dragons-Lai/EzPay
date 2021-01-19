all: client.cpp server.cpp
	g++ -o client client.cpp -L/usr/local/Cellar/openssl@1.1/1.1.1i/lib -lssl -lcrypto
	g++ -o server server.cpp -lpthread -L/usr/local/Cellar/openssl@1.1/1.1.1i/lib -lssl -lcrypto
clean: 
	rm -f client
	rm -f server