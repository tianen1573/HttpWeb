bin=httpserver
cgi=test_cgi
cc=g++
LD_FLAGS=-std=c++11 -lpthread
curr=$(shell pwd)
src=main.cc

ALL:$(bin) $(cgi)
.PHONY:ALL

$(bin):$(src)
	$(cc) -o $@ $^ $(LD_FLAGS)
$(cgi):cgi/test_cgi.cc
	$(cc) -o $@ $^ -std=c++11

.PHONY : clean
clean:
	rm -rf $(bin) $(cgi)
	rm -rf output

.PHONY:output
output:
	mkdir -p output
	cp $(bin) output
	cp -rf wwwroot output
	cp $(cgi) output/wwwroot


# POST /test_cgi HTTP/1.0
# Content-Length: 11
# Content-Type: text/html

# a=100&b=200

# get /test_cgi?a=100&b=200 HTTP/1.0

