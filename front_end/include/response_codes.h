#include "../utils/include/fe_utils.h"
#include "../../http_server/include/http_server.h"

/// @brief 503 service unavailable
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_503_page(const HttpRequest &req, HttpResponse &res);

/// @brief 502 internal server error
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_502_page(const HttpRequest &req, HttpResponse &res);

/// @brief 500 internal server error
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_500_page(const HttpRequest &req, HttpResponse &res);

/// @brief 409 conflict page
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_409_page(const HttpRequest &req, HttpResponse &res);

/// @brief 404 not found (valid request)
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_404_page(const HttpRequest &req, HttpResponse &res);

/// @brief 401 unauthorized page
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_401_page(const HttpRequest &req, HttpResponse &res);

/// @brief 400 bad request page
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_400_page(const HttpRequest &req, HttpResponse &res);
