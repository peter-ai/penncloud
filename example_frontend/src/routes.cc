#include "../../http_server/include/http_server.h"
#include "../../http_server/include/http_request.h"
#include "../../http_server/include/http_response.h"

#

void test_route(const HttpRequest& req, HttpResponse& res) {
    // std::vector<std::string> headers = req.get_header("Cookie");
    // for (int i=0; i < headers.size(); i++) std::cerr << "Entry " << i << ": " << headers[i] << std::endl;

    res.append_body_str("dynamic route working!");
    res.set_header("Content-Type", "text/plain");
    res.set_header("Content-Length", res.body_size());
    // res.set_header("Set-Cookie", "time=14:55pm");
    // res.set_header("Set-Cookie", "user=peter");
}

int main()
{
    auto test_route_handler = std::bind(&test_route, std::placeholders::_1, std::placeholders::_2);
    HttpServer::get("/api/test", test_route_handler);
    HttpServer::run(8000);
    return(0);
}