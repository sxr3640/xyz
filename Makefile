Main:
	g++ server/server.cpp libs/middle.cpp -o bin/server
	g++ libs/middle.cpp client/client.cpp  -o bin/client
	#g++ result/result.cpp libs/middle.cpp -o bin/result
clean:
	rm -f bin/*
