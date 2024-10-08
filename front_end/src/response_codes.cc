#include "../include/response_codes.h"

/// @brief 502 bad gateway
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_502_page(const HttpRequest &req, HttpResponse &res)
{
    std::string page =
        "<!doctype html>"
        "<html lang='en' data-bs-theme='dark'>"
        "<head>"
        "<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
        "<meta content='utf-8' http-equiv='encoding'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<meta name='description' content='CIS 5050 Spr24'>"
        "<meta name='keywords' content='LoginPage'>"
        "<title>Login - PennCloud.com</title>"
        "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
        "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
        "integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
        "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
        "</head>"
        "<body onload='setTheme()'>"
        "<nav class='navbar bg-body-tertiary'>"
        "<div class='container-fluid'>"
        "<span class='navbar-brand mb-0 h1 flex-grow-1'>"
        "<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
        "class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
        "<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
        "</svg>"
        "<span> </span>"
        "PennCloud"
        "</span>"
        "<div class='form-check form-switch form-check-reverse'>"
        "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
        "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
        "</div>"
        "</div>"
        "</nav>"
        "<div class='container-fluid text-start'>"
        "<div class='row mx-2 mt-3'>"
        "<h1 class='display-1'>"
        "502 - Bad gateway!"
        "</h1>"
        "<p class='lead'>Penncloud was unable to send email to external recipients, redirecting to home page in <span id='counter'>5</span>...</p>"
        "</div>"
        "</div>"
        "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
        "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
        "crossorigin='anonymous'></script>"
        "<script>"
        "document.getElementById('flexSwitchCheckReverse').addEventListener('change', () => {"
        "if (document.documentElement.getAttribute('data-bs-theme') === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "sessionStorage.setItem('data-bs-theme', 'light');"
        ""
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "sessionStorage.setItem('data-bs-theme', 'dark');"
        "}"
        "});"
        "</script>"
        "<script>"
        "function setTheme() {"
        "var theme = sessionStorage.getItem('data-bs-theme');"
        "if (theme !== null) {"
        "if (theme === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', true);"
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', false);"
        "}"
        "}"
        "};"
        "</script>"
        "<script>"
        "setInterval(function () {"
        "var count=Number($('#counter').text());"
        "if (count === 1)"
        "{"
        "window.location.replace('/home');"
        "}"
        "else"
        "{"
        "$('#counter').text(count-1);"
        "}"
        "}, 1000);"
        "</script>"
        "</body>"
        "</html>";
    res.set_code(502);
    res.append_body_str(page);
    res.set_header("Content-Type", "text/html");
}

/// @brief 500 internal server error
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_500_page(const HttpRequest &req, HttpResponse &res)
{
    std::string page =
        "<!doctype html>"
        "<html lang='en' data-bs-theme='dark'>"
        "<head>"
        "<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
        "<meta content='utf-8' http-equiv='encoding'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<meta name='description' content='CIS 5050 Spr24'>"
        "<meta name='keywords' content='LoginPage'>"
        "<title>Login - PennCloud.com</title>"
        "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
        "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
        "integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
        "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
        "</head>"
        "<body onload='setTheme()'>"
        "<nav class='navbar bg-body-tertiary'>"
        "<div class='container-fluid'>"
        "<span class='navbar-brand mb-0 h1 flex-grow-1'>"
        "<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
        "class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
        "<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
        "</svg>"
        "<span> </span>"
        "PennCloud"
        "</span>"
        "<div class='form-check form-switch form-check-reverse'>"
        "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
        "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
        "</div>"
        "</div>"
        "</nav>"
        "<div class='container-fluid text-start'>"
        "<div class='row mx-2 mt-3'>"
        "<h1 class='display-1'>"
        "500 - Internal server error!"
        "</h1>"
        "<p class='lead'>The server had trouble fulfilling your request as one or more email addresses are not valid, redirecting to home page in <span id='counter'>5</span>...</p>"
        "</div>"
        "</div>"
        "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
        "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
        "crossorigin='anonymous'></script>"
        "<script>"
        "document.getElementById('flexSwitchCheckReverse').addEventListener('change', () => {"
        "if (document.documentElement.getAttribute('data-bs-theme') === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "sessionStorage.setItem('data-bs-theme', 'light');"
        ""
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "sessionStorage.setItem('data-bs-theme', 'dark');"
        "}"
        "});"
        "</script>"
        "<script>"
        "function setTheme() {"
        "var theme = sessionStorage.getItem('data-bs-theme');"
        "if (theme !== null) {"
        "if (theme === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', true);"
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', false);"
        "}"
        "}"
        "};"
        "</script>"
        "<script>"
        "setInterval(function () {"
        "var count=Number($('#counter').text());"
        "if (count === 1)"
        "{"
        "window.location.replace('/home');"
        "}"
        "else"
        "{"
        "$('#counter').text(count-1);"
        "}"
        "}, 1000);"
        "</script>"
        "</body>"
        "</html>";
    res.set_code(500);
    res.append_body_str(page);
    res.set_header("Content-Type", "text/html");
}


/// @brief 409 conflict page
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_409_page(const HttpRequest &req, HttpResponse &res)
{
    std::string page =
        "<!doctype html>"
        "<html lang='en' data-bs-theme='dark'>"
        "<head>"
        "<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
        "<meta content='utf-8' http-equiv='encoding'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<meta name='description' content='CIS 5050 Spr24'>"
        "<meta name='keywords' content='LoginPage'>"
        "<title>Login - PennCloud.com</title>"
        "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
        "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
        "integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
        "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
        "</head>"
        "<body onload='setTheme()'>"
        "<nav class='navbar bg-body-tertiary'>"
        "<div class='container-fluid'>"
        "<span class='navbar-brand mb-0 h1 flex-grow-1'>"
        "<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
        "class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
        "<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
        "</svg>"
        "<span> </span>"
        "PennCloud"
        "</span>"
        "<div class='form-check form-switch form-check-reverse'>"
        "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
        "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
        "</div>"
        "</div>"
        "</nav>"
        "<div class='container-fluid text-start'>"
        "<div class='row mx-2 mt-3'>"
        "<h1 class='display-1'>"
        "409 - Conflict!"
        "</h1>"
        "<p class='lead'>The account request already exists, redirecting to login in <span id='counter'>5</span>...</p>"
        "</div>"
        "</div>"
        "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
        "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
        "crossorigin='anonymous'></script>"
        "<script>"
        "document.getElementById('flexSwitchCheckReverse').addEventListener('change', () => {"
        "if (document.documentElement.getAttribute('data-bs-theme') === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "sessionStorage.setItem('data-bs-theme', 'light');"
        ""
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "sessionStorage.setItem('data-bs-theme', 'dark');"
        "}"
        "});"
        "</script>"
        "<script>"
        "function setTheme() {"
        "var theme = sessionStorage.getItem('data-bs-theme');"
        "if (theme !== null) {"
        "if (theme === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', true);"
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', false);"
        "}"
        "}"
        "};"
        "</script>"
        "<script>"
        "setInterval(function () {"
        "var count=Number($('#counter').text());"
        "if (count === 1)"
        "{"
        "window.location.replace('http://127.0.0.1:7500');"
        "}"
        "else"
        "{"
        "$('#counter').text(count-1);"
        "}"
        "}, 1000);"
        "</script>"
        "</body>"
        "</html>";
    res.set_code(409);
    res.append_body_str(page);
    res.set_header("Content-Type", "text/html");
}

/// @brief 404 not found (valid request)
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_404_page(const HttpRequest &req, HttpResponse &res)
{
    std::string page =
        "<!doctype html>"
        "<html lang='en' data-bs-theme='dark'>"
        "<head>"
        "<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
        "<meta content='utf-8' http-equiv='encoding'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<meta name='description' content='CIS 5050 Spr24'>"
        "<meta name='keywords' content='LoginPage'>"
        "<title>Login - PennCloud.com</title>"
        "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
        "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
        "integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
        "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
        "</head>"
        "<body onload='setTheme()'>"
        "<nav class='navbar bg-body-tertiary'>"
        "<div class='container-fluid'>"
        "<span class='navbar-brand mb-0 h1 flex-grow-1'>"
        "<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
        "class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
        "<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
        "</svg>"
        "<span> </span>"
        "PennCloud"
        "</span>"
        "<div class='form-check form-switch form-check-reverse'>"
        "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
        "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
        "</div>"
        "</div>"
        "</nav>"
        "<div class='container-fluid text-start'>"
        "<div class='row mx-2 mt-3'>"
        "<h1 class='display-1'>"
        "404 - Not Found!"
        "</h1>"
        "<p class='lead'>This page doesn't exist, redirecting to home in <span id='counter'>5</span>...</p>"
        "</div>"
        "</div>"
        "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
        "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
        "crossorigin='anonymous'></script>"
        "<script>"
        "document.getElementById('flexSwitchCheckReverse').addEventListener('change', () => {"
        "if (document.documentElement.getAttribute('data-bs-theme') === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "sessionStorage.setItem('data-bs-theme', 'light');"
        ""
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "sessionStorage.setItem('data-bs-theme', 'dark');"
        "}"
        "});"
        "</script>"
        "<script>"
        "function setTheme() {"
        "var theme = sessionStorage.getItem('data-bs-theme');"
        "if (theme !== null) {"
        "if (theme === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', true);"
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', false);"
        "}"
        "}"
        "};"
        "</script>"
        "<script>"
        "setInterval(function () {"
        "var count=Number($('#counter').text());"
        "if (count === 1)"
        "{"
        "window.location.replace('/home');"
        "}"
        "else"
        "{"
        "$('#counter').text(count-1);"
        "}"
        "}, 1000);"
        "</script>"
        "</body>"
        "</html>";
    res.set_code(404);
    res.append_body_str(page);
    res.set_header("Content-Type", "text/html");
}

/// @brief 401 unauthorized page
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_401_page(const HttpRequest &req, HttpResponse &res)
{
    std::string page =
        "<!doctype html>"
        "<html lang='en' data-bs-theme='dark'>"
        "<head>"
        "<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
        "<meta content='utf-8' http-equiv='encoding'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<meta name='description' content='CIS 5050 Spr24'>"
        "<meta name='keywords' content='LoginPage'>"
        "<title>Login - PennCloud.com</title>"
        "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
        "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
        "integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
        "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
        "</head>"
        "<body onload='setTheme()'>"
        "<nav class='navbar bg-body-tertiary'>"
        "<div class='container-fluid'>"
        "<span class='navbar-brand mb-0 h1 flex-grow-1'>"
        "<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
        "class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
        "<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
        "</svg>"
        "<span> </span>"
        "PennCloud"
        "</span>"
        "<div class='form-check form-switch form-check-reverse'>"
        "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
        "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
        "</div>"
        "</div>"
        "</nav>"
        "<div class='container-fluid text-start'>"
        "<div class='row mx-2 mt-3'>"
        "<h1 class='display-1'>"
        "401 - Unauthorized!"
        "</h1>"
        "<p class='lead'>The active session is no longer valid, redirecting to login in <span id='counter'>5</span>...</p>"
        "</div>"
        "</div>"
        "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
        "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
        "crossorigin='anonymous'></script>"
        "<script>"
        "document.getElementById('flexSwitchCheckReverse').addEventListener('change', () => {"
        "if (document.documentElement.getAttribute('data-bs-theme') === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "sessionStorage.setItem('data-bs-theme', 'light');"
        ""
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "sessionStorage.setItem('data-bs-theme', 'dark');"
        "}"
        "});"
        "</script>"
        "<script>"
        "function setTheme() {"
        "var theme = sessionStorage.getItem('data-bs-theme');"
        "if (theme !== null) {"
        "if (theme === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', true);"
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', false);"
        "}"
        "}"
        "};"
        "</script>"
        "<script>"
        "setInterval(function () {"
        "var count=Number($('#counter').text());"
        "if (count === 1)"
        "{"
        "window.location.replace('http://127.0.0.1:7500');"
        "}"
        "else"
        "{"
        "$('#counter').text(count-1);"
        "}"
        "}, 1000);"
        "</script>"
        "</body>"
        "</html>";
    res.set_code(401);
    res.append_body_str(page);
    res.set_header("Content-Type", "text/html");
}

/// @brief 400 bad request page
/// @param req HttpRequest object
/// @param res HttpResponse object
void error_400_page(const HttpRequest &req, HttpResponse &res)
{
    std::string page =
        "<!doctype html>"
        "<html lang='en' data-bs-theme='dark'>"
        "<head>"
        "<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
        "<meta content='utf-8' http-equiv='encoding'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<meta name='description' content='CIS 5050 Spr24'>"
        "<meta name='keywords' content='LoginPage'>"
        "<title>Login - PennCloud.com</title>"
        "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
        "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
        "integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
        "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
        "</head>"
        "<body onload='setTheme()'>"
        "<nav class='navbar bg-body-tertiary'>"
        "<div class='container-fluid'>"
        "<span class='navbar-brand mb-0 h1 flex-grow-1'>"
        "<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
        "class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
        "<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
        "</svg>"
        "<span> </span>"
        "PennCloud"
        "</span>"
        "<div class='form-check form-switch form-check-reverse'>"
        "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
        "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
        "</div>"
        "</div>"
        "</nav>"
        "<div class='container-fluid text-start'>"
        "<div class='row mx-2 mt-3'>"
        "<h1 class='display-1'>"
        "400 - Bad request!"
        "</h1>"
        "<p class='lead'>Redirecting to login in <span id='counter'>5</span>...</p>"
        "</div>"
        "</div>"
        "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
        "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
        "crossorigin='anonymous'></script>"
        "<script>"
        "document.getElementById('flexSwitchCheckReverse').addEventListener('change', () => {"
        "if (document.documentElement.getAttribute('data-bs-theme') === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "sessionStorage.setItem('data-bs-theme', 'light');"
        ""
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "sessionStorage.setItem('data-bs-theme', 'dark');"
        "}"
        "});"
        "</script>"
        "<script>"
        "function setTheme() {"
        "var theme = sessionStorage.getItem('data-bs-theme');"
        "if (theme !== null) {"
        "if (theme === 'dark') {"
        "document.documentElement.setAttribute('data-bs-theme', 'dark');"
        "$('#switchLabel').html('Dark Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', true);"
        "}"
        "else {"
        "document.documentElement.setAttribute('data-bs-theme', 'light');"
        "$('#switchLabel').html('Light Mode');"
        "$('#flexSwitchCheckReverse').attr('checked', false);"
        "}"
        "}"
        "};"
        "</script>"
        "<script>"
        "setInterval(function () {"
        "var count=Number($('#counter').text());"
        "if (count === 1)"
        "{"
        "window.location.replace('http://127.0.0.1:7500');"
        "}"
        "else"
        "{"
        "$('#counter').text(count-1);"
        "}"
        "}, 1000);"
        "</script>"
        "</body>"
        "</html>";
    res.set_code(400);
    res.append_body_str(page);
    res.set_header("Content-Type", "text/html");
}