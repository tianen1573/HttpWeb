bin=httpserver
cc=g++
LD_FLAGS=-std=c++11 -lpthread
curr=$(shell pwd)
src=main.cc

.PHONY:ALL
ALL:$(bin) CGI output

$(bin):$(src)
	$(cc) -o $@ $^ $(LD_FLAGS)
CGI:
	cd $(curr)/cgi;\
	make;\
	cd -;

.PHONY:output
output:
	mkdir -p output
	cp $(bin) output
	cp -rf wwwroot output
	cp ./cgi/test_cgi output/wwwroot
	cp ./cgi/shell_cgi.sh output/wwwroot
	cp ./cgi/mysql_cgi output/wwwroot

.PHONY : clean
clean:
	rm -rf $(bin)
	rm -rf output
	cd $(curr)/cgi;\
	make clean;\
	cd -;

# POST /test_cgi HTTP/1.0
# Content-Length: 11
# Content-Type: text/html

# a=100&b=200

# telnet 127.0.0.1 8081

# get /test_cgi?a=100&b=200 HTTP/1.0

