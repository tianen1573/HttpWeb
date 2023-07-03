#include <iostream>
#include <cstring>
#include <memory>
#include "HttpServer.hpp"
#include "Log.hpp"

static void Usage(std::string proc)
{
    std::cout << "Usage:\n\t" << proc << " port" << std::endl;
}

int main(int argc, char * argv[])
{

    // LOG(DEBUG, "dubug Log.hpp");

    if(argc != 2)
    {
        Usage(argv[0]);
        exit(4);
    }

    int port = atoi(argv[1]);
    
    std::shared_ptr<HttpServer> http_server(new HttpServer(port));//RAII

    http_server->InitServer();
    http_server->Loop();

    return 0;
}