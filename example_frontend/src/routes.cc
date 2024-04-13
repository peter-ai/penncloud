#include "../../http_server/include/http_server.h"
#include "../../http_server/include/http_request.h"
#include "../../http_server/include/http_response.h"

#

void test_route(const HttpRequest& req, HttpResponse& res) {
    res.append_body_str("dynamic route working!");
    res.set_header("Content-Type", "text/plain");
    res.set_header("Content-Length", res.body_size());
    res.set_cookie("user", "me");
    res.set_cookie("sid", "123");

    // std::vector<std::string> cookies = req.get_header("Cookie"); 
    // for (int i=0; i < cookies.size(); i++) std::cerr << "Entry #" << i+1 << " " << cookies[i] << std::endl;

    // std::vector<std::string> encodings = req.get_header("Accept-Encoding"); 
    // for (int i=0; i < encodings.size(); i++) std::cerr << "Entry #" << i+1 << " " << encodings[i] << std::endl;
}

int main()
{
    auto test_route_handler = std::bind(&test_route, std::placeholders::_1, std::placeholders::_2);
    HttpServer::get("/api/test", test_route_handler);
    HttpServer::run(8000);
    return(0);
}