#include "http_server/include/http_server.h"
#include "http_server/include/http_request.h"
#include "http_server/include/http_response.h"

void test_route(HttpRequest req, HttpResponse res) {
    res.append_body_str("dynamic route working!");
}

int main()
{
    HttpServer::get("/api/test", test_route);
    HttpServer::run(8000);
    return(0);
}