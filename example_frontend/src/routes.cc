#include "../../http_server/include/http_server.h"
#include "../../http_server/include/http_request.h"
#include "../../http_server/include/http_response.h"


void test_route(const HttpRequest& req, HttpResponse& res) {
    std::vector<char> req_body = req.body_as_bytes();
    res.append_body_bytes(req_body.data(), req_body.size());
    res.set_header("Content-Type", "image/jpeg");
}

int main()
{
    auto test_route_handler = std::bind(&test_route, std::placeholders::_1, std::placeholders::_2);
    HttpServer::post("/api/test", test_route_handler);
    HttpServer::run(8000);
    return(0);
}