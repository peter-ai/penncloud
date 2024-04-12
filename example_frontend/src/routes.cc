#include "../../http_server/include/http_server.h"
#include "../../http_server/include/http_request.h"
#include "../../http_server/include/http_response.h"

#

void test_route(const HttpRequest& req, HttpResponse& res) {
    res.append_body_str("dynamic route working!");
    res.set_header("Content-Type", "text/plain");
    res.set_header("Content-Length", res.body_size());
}

int main()
{
    auto test_route_handler = std::bind(&test_route, std::placeholders::_1, std::placeholders::_2);
    HttpServer::post("/api/test", test_route_handler);
    HttpServer::run(8000);
    return(0);
}