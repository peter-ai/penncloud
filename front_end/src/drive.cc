/*
 * drive.cc
 *
 *  Created on: Apr 10, 2024
 *      Author: aashok12
 */
#include "../include/drive.h"

// Folder handlers

std::vector<char> ok_vec = {'+', 'O', 'K', ' '};
std::vector<char> err_vec = {'-', 'E', 'R', ' '};

// helper to return parent path
std::string split_parent_filename(const std::vector<std::string> &vec, std::string &filename)
{
    std::string parentpath;

    // Iterate over all elements except the last one
    for (size_t i = 0; i < vec.size() - 1; ++i)
    {
        parentpath += vec[i]; // Append the current element to the result string
        parentpath += '/';
    }

    filename = vec.back();

    return parentpath;
}

// checks if path ends in /, if yes folder. Otherwise of type file
bool is_folder(const std::vector<char> &vec)
{
    return vec.back() == '/';
}

bool kv_successful(const std::vector<char> &vec)
{
    // Check if the vector has at least 3 characters
    if (vec.size() < 3)
    {
        return false;
    }

    // Define the expected prefix
    std::vector<char> prefix = {'+', 'O', 'K'};

    // Check if the first three characters match the prefix
    return std::equal(prefix.begin(), prefix.end(), vec.begin());
}

// Function to split a vector<char> based on a vector<char> delimiter
std::vector<std::vector<char>> split_vector(const std::vector<char> &data, const std::vector<char> &delimiter)
{
    std::vector<std::vector<char>> result;
    size_t start = 0;
    size_t end = data.size();

    while (start < end)
    {
        // Find the next occurrence of delimiter starting from 'start'
        auto pos = std::search(data.begin() + start, data.end(), delimiter.begin(), delimiter.end());

        if (pos == data.end())
        {
            // No delimiter found, copy the rest of the vector
            result.emplace_back(data.begin() + start, data.end());
            break;
        }
        else
        {
            // Delimiter found, copy up to the delimiter and move 'start' past the delimiter
            result.emplace_back(data.begin() + start, pos);
            start = std::distance(data.begin(), pos) + delimiter.size();
        }
    }

    return result;
}

// Function to split a vector<char> based on the first occurrence of a vector<char> delimiter
std::vector<std::vector<char>> split_vec_first_delim(const std::vector<char> &data, const std::vector<char> &delimiter)
{
    std::vector<std::vector<char>> result;
    size_t start = 0;

    // Find the first occurrence of delimiter in data
    auto pos = std::search(data.begin() + start, data.end(), delimiter.begin(), delimiter.end());

    if (pos == data.end())
    {
        // No delimiter found, return the whole vector as a single part
        result.emplace_back(data.begin(), data.end());
    }
    else
    {
        // Delimiter found, split at the delimiter
        result.emplace_back(data.begin() + start, pos);          // Part before the delimiter
        result.emplace_back(pos + delimiter.size(), data.end()); // Part after the delimiter
    }

    return result;
}

// helper to return parent path
std::vector<char> format_folder_contents(std::vector<std::vector<char>> &vec)
{
    std::vector<char> output;

    // Iterate over all elements except the last one
    for (size_t i = 0; i < vec.size() - 1; ++i)
    {
        output.insert(output.end(), (vec[i]).begin(), (vec[i]).end());
        output.push_back(',');
        output.push_back(' ');
    }

    output.insert(output.end(), (vec.back()).begin(), (vec.back()).end());

    return output;
}
//3431416482696731938431374517029964808742144996087957558348317691
void open_filefolder(const HttpRequest &req, HttpResponse &res)
{
    // @PETER ADDED
    std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(req);
    // TODO: @PETER ADDED - EVERY HANDLER MUST CHECK COOKIES TO MAKE SURE USER IS VALID

    if (cookies.count("user") && cookies.count("sid"))
    {
        std::string username = cookies["user"];
        std::string sid = cookies["sid"];

        // check req method

        // path is /api/drive/:childpath where parent dir is the page that is being displayed

        std::string childpath_str = req.path.substr(7);
        std::vector<char> child_path(childpath_str.begin(), childpath_str.end());
        int sockfd = FeUtils::open_socket();

        // if we are looking up a folder, use get row
        if (is_folder(child_path))
        {

            std::vector<char> folder_content = FeUtils::kv_get_row(sockfd, child_path);

            if (kv_successful(folder_content))
            {
                // content list, remove '+OK<sp>'
                std::vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
                // split on delim
                std::vector<std::vector<char>> contents = split_vector(folder_elements, {'\b'});
                std::vector<char> formatted_content = format_folder_contents(contents);

                // @PETER ADDED
                std::string folder_contents(formatted_content.begin(), formatted_content.end());
                std::vector<std::string> folder_items = Utils::split(folder_contents, ", ");
                std::string folder_html = "";
                size_t item_iter;
                for (item_iter = 0; item_iter < folder_items.size(); item_iter++)
                {

                    std::string item = folder_items[item_iter];

                    // start row
                    if (item_iter % 9 == 0)
                    {
                        folder_html += "<div class='row mx-2 mt-2 align-items-start'>";
                    }

                    if (item_iter % 9 == 0 || item_iter % 9 == 3 || item_iter % 9 == 6)
                    {
                        folder_html += "<div class='col-4'><div class='row align-items-start'>";
                    }

                    // html to add item to page
                    if (item.back() == '/')
                    {
                        folder_html +=
                            "<div class='col-4 text-center text-wrap'>"
                            "<svg xmlns='http://www.w3.org/2000/svg' width='100%' height='100%' fill='currentColor' class='bi bi-folder-fill' viewBox='0 0 16 16'>"
                            "<path d='M9.828 3h3.982a2 2 0 0 1 1.992 2.181l-.637 7A2 2 0 0 1 13.174 14H2.825a2 2 0 0 1-1.991-1.819l-.637-7a2 2 0 0 1 .342-1.31L.5 3a2 2 0 0 1 2-2h3.672a2 2 0 0 1 1.414.586l.828.828A2 2 0 0 0 9.828 3m-8.322.12q.322-.119.684-.12h5.396l-.707-.707A1 1 0 0 0 6.172 2H2.5a1 1 0 0 0-1 .981z'/>"
                            "</svg>"
                            "<p class='lead text-break'>" +
                            item +
                            "</p>"
                            "</div>";
                    }
                    else if (item.compare("sid") == 0 || item.compare("pass") == 0)
                    {
                        continue;
                    }
                    else
                    {
                        folder_html +=
                            "<div class='col-4 text-center text-wrap'>"
                            "<svg xmlns='http://www.w3.org/2000/svg' width='100%' height='100%' fill='currentColor' class='bi bi-file-earmark-fill' viewBox='0 0 16 16'>"
                            "<path d='M4 0h5.293A1 1 0 0 1 10 .293L13.707 4a1 1 0 0 1 .293.707V14a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V2a2 2 0 0 1 2-2m5.5 1.5v2a1 1 0 0 0 1 1h2z'/>"
                            "</svg>"
                            "<p class='lead text-break'>" +
                            item +
                            "</p>"
                            "</div>";
                    }

                    if (item_iter % 9 == 2 || item_iter % 9 == 5 || item_iter % 9 == (9 - 1))
                    {
                        folder_html += "</div></div>";
                    }

                    if (item_iter % 9 == (9 - 1))
                    {
                        folder_html += "</div>";
                    }
                }
                if (item_iter % 9 != (9 - 1))
                    folder_html += "</div>";

                //@todo: update with html!
                std::string page =
                    "<!doctype html>"
                    "<html lang='en' data-bs-theme='dark'>"
                    "<head>"
                    "<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
                    "<meta content='utf-8' http-equiv='encoding'>"
                    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                    "<meta name='description' content='CIS 5050 Spr24'>"
                    "<meta name='keywords' content='Home'>"
                    "<title>Home - PennCloud.com</title>"
                    "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
                    "integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
                    "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
                    "</head>"

                    "<body onload='setTheme()'>"
                    "<nav class='navbar navbar-expand-lg bg-body-tertiary'>"
                    "<div class='container-fluid'>"
                    "<span class='navbar-brand mb-0 h1 flex-grow-1'>"
                    "<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
                    "class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
                    "<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
                    "</svg>"
                    "PennCloud"
                    "</span>"
                    "<button class='navbar-toggler' type='button' data-bs-toggle='collapse' data-bs-target='#navbarNavAltMarkup' aria-controls='navbarNavAltMarkup' aria-expanded='false' aria-label='Toggle navigation'>"
                    "<span class='navbar-toggler-icon'></span>"
                    "</button>"
                    "<div class='collapse navbar-collapse' id='navbarNavAltMarkup'>"
                    "<div class='navbar-nav'>"
                    "<a class='nav-link' href='/home'>Home</a>"
                    "<a class='nav-link active' aria-current='page' href='/drive/" +
                    username + "/'>Drive</a>"
                               "<a class='nav-link' href='/" +
                    username + "/mbox'>Email</a>"
                               "<a class='nav-link disabled' aria-disabled='true'>Games</a>"
                               "<a class='nav-link' href='/account'>Account</a>"
                               "<form class='d-flex' role='form' method='POST' action='/api/logout'>"
                               "<input type='hidden' />"
                               "<button class='btn nav-link' type='submit'>Logout</button>"
                               "</form>"
                               "</div>"
                               "</div>"
                               "<div class='form-check form-switch form-check-reverse'>"
                               "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
                               "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
                               "</div>"
                               "</div>"
                               "</nav>"

                               "<div class='container-fluid text-start'>"
                               "<div class='row mx-2 mt-3 mb-4'>"
                               "<h1 class='display-6'>"
                               "Drive: " +
                    childpath_str +
                    "</h1>"
                    "</div>" +
                    folder_html +
                    "</div>"

                    "<script src='http://ajax.googleapis.com/ajax/libs/jquery/2.0.3/jquery.min.js'></script>"
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
                    "</body>"
                    "</html>";

                // @PETER ADDED - reset cookies of user
                FeUtils::set_cookies(res, username, sid);

                res.append_body_str(page);
                // res.append_body_bytes(formatted_content.data(), formatted_content.size());

                // append header for content length
                res.set_code(200);
            }
            else
            {
                // @PETER ADDED
                // set response status code
                res.set_code(303);

                FeUtils::expire_cookies(res, username, sid);

                // set response headers / redirect to 400 error
                res.set_header("Content-Type", "text/html");
                res.set_header("Location", "/400");
            }
        }
        else
        {
            // file, need to get parent row and file name
            std::string filename;
            std::string parentpath_str = split_parent_filename(Utils::split(childpath_str, "/"), filename);

            std::vector<char> parent_path_vec(parentpath_str.begin(), parentpath_str.end());
            std::vector<char> filename_vec(filename.begin(), filename.end());

            // get file content
            std::vector<char> file_content = FeUtils::kv_get(sockfd, parent_path_vec, filename_vec);

            if (kv_successful(file_content))
            {
                // get binary from 4th char onward (ignore +OK<sp>)
                std::vector<char> file_binary(file_content.begin() + 4, file_content.end());

                // apend to body
                res.append_body_bytes(file_binary.data(), file_binary.size());

                // // octet-steam for content header @todo -- setting this type means postman can't see it
                // std::string content_header = "Content-Type";
                // std::string content_value = "application/octet-stream";
                // res.set_header(content_header, content_value);

                // @PETER ADDED - reset cookies of user
                FeUtils::set_cookies(res, cookies["user"], cookies["sid"]);

                // set code
                res.set_code(200);
            }
            else
            {
                // @todo ask about error codes
                // @PETER ADDED
                // set response status code
                res.set_code(303);

                // set response headers / redirect to 400 error
                res.set_header("Content-Type", "text/html");
                res.set_header("Location", "/400");
            }
        }

        close(sockfd);
    }
    else
    {
        // set response status code
        res.set_code(303);

        // set response headers / redirect to 401 error
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/401");
    }
}

void upload_file(const HttpRequest &req, HttpResponse &res)
{

    // Check if the request contains a body
    if (!req.body_as_bytes().empty())
    {

        // ------- Get file name and clean up to get file binary ----
        // Get the single file uploaded
        std::vector<std::string> headers;

        // Find form boundary
        headers = req.get_header("Content-Type");
        std::string header_str(headers[0]);
        // boundary provided by form
        std::vector<std::string> find_boundary = Utils::split_on_first_delim(header_str, "boundary=");
        std::vector<char> bound_vec(find_boundary.back().begin(), find_boundary.back().end());

        std::vector<char> line_feed = {'\r', '\n'};

        // split body on boundary
        std::vector<std::vector<char>> elements = split_vector(req.body_as_bytes(), bound_vec);
        std::vector<char> elem1 = elements[1];

        // @note: assuming we only upload 1 file at a time?
        std::vector<char> file_data = split_vec_first_delim(elem1, line_feed)[1];

        // split file data to separate file metadata from binary values
        std::vector<std::vector<char>> body_elems = split_vec_first_delim(file_data, line_feed);

        // parse file headers to get name of file
        std::string file_headers(body_elems[0].begin(), body_elems[0].end());
        std::string content_disp = Utils::split_on_first_delim(file_headers, "\n")[0];
        std::string filename_toparse = Utils::split_on_first_delim(file_headers, "filename=").back();

        // file name string
        std::string filename = Utils::split(filename_toparse, "\"")[0];

        // get file binary
        std::vector<char> file_binary = split_vec_first_delim(body_elems[1], line_feed)[1];
        file_binary = split_vec_first_delim(file_binary, line_feed)[1];

        // Get path of parent directory where we are appending

        // path is /api/drive/upload/:parentpath where parent dir is the page that is being displayed
        std::string parentpath_str = req.path.substr(18);
        std::string childpath_str = parentpath_str + filename;

        std::vector<char> row_vec(parentpath_str.begin(), parentpath_str.end());
        std::vector<char> col_vec(filename.begin(), filename.end());

        int sockfd = FeUtils::open_socket();

        std::vector<char> kvs_resp = FeUtils::kv_put(sockfd, row_vec, col_vec, file_binary);

        if (kv_successful(kvs_resp))
        {
            // @todo should we instead get row for the page they are on?
            res.set_code(200); // OK
            std::vector<char> folder_contents = FeUtils::kv_get_row(sockfd, row_vec);
            res.append_body_bytes(folder_contents.data(), folder_contents.size());
        }
        else
        {
            res.set_code(400);
            // maybe retry? tbd
        }

        // @todo should we instead get row for the page they are on?
    }
    else
    {
        // No body found in the request
        res.set_code(400); // Bad Request
    }
}