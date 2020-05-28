
#include "client.h"


void Client::read_from_network(ssize_t size, char* buf)
{
    parser.readFromNetwork(buf, size);
}

void Client::processing_request(Request& req, Response& resp)
{
    whs->processing_request(req, resp);
}

void Client::write_response(Response& resp)
{
    char* buf;
    size_t size;
    resp.toBytes(&buf, size);
    whs->write(this, buf, size);
}
