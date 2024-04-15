#include "../../http_server/include/http_server.h"
#include "../../http_server/include/http_request.h"
#include "../../http_server/include/http_response.h"
#include "../../front_end/include/drive.h"


void test_route(const HttpRequest& req, HttpResponse& res) {
    std::vector<char> req_body = req.body_as_bytes();
    res.append_body_bytes(req_body.data(), req_body.size());
    res.set_header("Content-Type", "image/jpeg");
}


void test_dynamic(const HttpRequest& req, HttpResponse& res) {
    res.append_body_str("dynamic matching working!");
    res.append_body_str(req.get_qparam("user"));
    res.append_body_str(req.get_qparam("pw"));
    res.set_header("Content-Type", "text/plain");
}

int main()
{
    auto test_dynamic_handler = std::bind(&test_dynamic, std::placeholders::_1, std::placeholders::_2);
    HttpServer::get("/api/test/*", test_dynamic_handler);

    auto test_route_handler = std::bind(&test_route, std::placeholders::_1, std::placeholders::_2);
    HttpServer::get("/api/test", test_route_handler);

    auto test_getfile_handler = std::bind(&open_filefolder, std::placeholders::_1, std::placeholders::_2);
    HttpServer::get("/api/drive/*", test_getfile_handler);

    auto upload_file_handler = std::bind(&upload_file, std::placeholders::_1, std::placeholders::_2);
    HttpServer::post("/api/drive/upload/*", upload_file_handler);


    HttpServer::run(8000);
    return(0);
}