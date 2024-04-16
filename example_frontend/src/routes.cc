// #include "../../http_server/include/http_server.h"
// #include "../../http_server/include/http_request.h"
// #include "../../http_server/include/http_response.h"


// void test_route(const HttpRequest& req, HttpResponse& res) {
//   // std::vector<std::string> headers = req.get_header("Cookie");
//     // for (int i=0; i < headers.size(); i++) std::cerr << "Entry " << i << ": " << headers[i] << std::endl;

//     res.append_body_str("dynamic route working!");
//     res.set_header("Content-Type", "text/plain");
//     res.set_cookie("user", "me");
//     res.set_cookie("sid", "123");

//     // std::vector<std::string> cookies = req.get_header("Cookie"); 
//     // for (int i=0; i < cookies.size(); i++) std::cerr << "Entry #" << i+1 << " " << cookies[i] << std::endl;

//     // std::vector<std::string> encodings = req.get_header("Accept-Encoding"); 
//     // for (int i=0; i < encodings.size(); i++) std::cerr << "Entry #" << i+1 << " " << encodings[i] << std::endl;
// }


// void test_dynamic(const HttpRequest& req, HttpResponse& res) {
//     res.append_body_str("dynamic matching working!");
//     res.append_body_str(req.get_qparam("user"));
//     res.append_body_str(req.get_qparam("pw"));
//     res.set_header("Content-Type", "text/plain");
// }

// int main()
// {
//     auto test_dynamic_handler = std::bind(&test_dynamic, std::placeholders::_1, std::placeholders::_2);
//     HttpServer::get("/api/test?", test_dynamic_handler);

//     auto test_route_handler = std::bind(&test_route, std::placeholders::_1, std::placeholders::_2);
//     HttpServer::get("/api/test", test_route_handler);

//     HttpServer::run(8000);
//     return(0);
// }

#include "../../http_server/include/http_server.h"
#include "../../http_server/include/http_request.h"
#include "../../http_server/include/http_response.h"


void test_route(const HttpRequest& req, HttpResponse& res) {
  // std::vector<std::string> headers = req.get_header("Cookie");
    // for (int i=0; i < headers.size(); i++) std::cerr << "Entry " << i << ": " << headers[i] << std::endl;

    res.append_body_str("dynamic route working!");
    res.set_header("Content-Type", "text/plain");

    
    std::string mailboxPath = req.path.substr(5);  // Assuming '/api/' is 5 characters

    // Extracting query parameter 'uidl'
    std::string uidl = req.get_qparam("uidl");

    // Respond with details extracted from the request
    res.append_body_str("Mailbox Path: " + mailboxPath + "\n");
    res.append_body_str("UIDL: " + uidl + "\n");
    res.append_body_str("Dynamic route working!\n");
    res.set_header("Content-Type", "text/plain");

    // std::vector<std::string> cookies = req.get_header("Cookie"); 
    // for (int i=0; i < cookies.size(); i++) std::cerr << "Entry #" << i+1 << " " << cookies[i] << std::endl;

    // std::vector<std::string> encodings = req.get_header("Accept-Encoding"); 
    // for (int i=0; i < encodings.size(); i++) std::cerr << "Entry #" << i+1 << " " << encodings[i] << std::endl;
}


void test_dynamic(const HttpRequest& req, HttpResponse& res) {
    res.append_body_str("dynamic matching working!");
    res.append_body_str(req.get_qparam("user"));
    res.append_body_str(req.get_qparam("pw"));
    res.set_header("Content-Type", "text/plain");
}

int main()
{
    // auto test_dynamic_handler = std::bind(&test_dynamic, std::placeholders::_1, std::placeholders::_2);
    // HttpServer::get("/api/test?", test_dynamic_handler);

    auto test_route_handler = std::bind(&test_route, std::placeholders::_1, std::placeholders::_2);
    HttpServer::get("/api/:user/mbox?", test_route_handler);

    //  auto test_route_handler = std::bind(&test_route, std::placeholders::_1, std::placeholders::_2);
    // HttpServer::get("/api/test", test_route_handler);
    HttpServer::run(8000);
    return(0);
}