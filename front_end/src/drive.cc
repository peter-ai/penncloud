/*
 * drive.cc
 *
 *  Created on: Apr 10, 2024
 *      Author: aashok12
 */
#include "../include/drive.h"
using namespace std;
// Folder handlers

vector<char> ok_vec = {'+', 'O', 'K', ' '};
vector<char> err_vec = {'-', 'E', 'R', ' '};

Logger logger("Drive");

// helper to return parent path
string split_parent_filename(const vector<string> &vec, string &filename)
{
    string parentpath;

    // Iterate over all elements except the last one
    for (size_t i = 0; i < vec.size() - 1; ++i)
    {
        parentpath += vec[i]; // Append the current element to the result string
        parentpath += '/';
    }

    filename = vec.back();

    return parentpath;
}

// gets username from file path
string get_username(const string path)
{
    return Utils::split_on_first_delim(path, "/")[0];
}

// helper to check if a vecotr of chars contains a subseqeunce
bool contains_subseq(const vector<char> &sequence, const vector<char> &subsequence)
{
    // convert both vectors to strings
    string seq_str(sequence.begin(), sequence.end());
    string subseq_str(subsequence.begin(), subsequence.end());

    // using search to find the subsequence in the sequence
    return search(seq_str.begin(), seq_str.end(), subseq_str.begin(), subseq_str.end()) != seq_str.end();
}

// checks if path ends in /, if yes folder. Otherwise of type file
bool is_folder(const vector<char> &vec)
{
    return vec.back() == '/';
}

bool kv_successful(const vector<char> &vec)
{
    // Check if the vector has at least 3 characters
    if (vec.size() < 3)
    {
        return false;
    }

    // Define the expected prefix
    vector<char> prefix = {'+', 'O', 'K'};

    // Check if the first three characters match the prefix
    return equal(prefix.begin(), prefix.end(), vec.begin());
}

// // Function to split a vector<char> based on a vector<char> delimiter
// vector<vector<char>> FeUtils::split_vector(const vector<char> &data, const vector<char> &delimiter)
// {
//     vector<vector<char>> result;
//     size_t start = 0;
//     size_t end = data.size();

//     if (data.size() == 0)
//     {
//         return {{}};
//     }

//     while (start < end)
//     {
//         // Find the next occurrence of delimiter starting from 'start'
//         auto pos = search(data.begin() + start, data.end(), delimiter.begin(), delimiter.end());

//         if (pos == data.end())
//         {
//             // No delimiter found, copy the rest of the vector
//             result.emplace_back(data.begin() + start, data.end());
//             break;
//         }
//         else
//         {
//             // Delimiter found, copy up to the delimiter and move 'start' past the delimiter
//             result.emplace_back(data.begin() + start, pos);
//             start = distance(data.begin(), pos) + delimiter.size();
//         }
//     }

//     return result;
// }

// Function to split a vector<char> based on the first occurrence of a vector<char> delimiter
vector<vector<char>> split_vec_first_delim(const vector<char> &data, const vector<char> &delimiter)
{
    vector<vector<char>> result;
    size_t start = 0;

    // Find the first occurrence of delimiter in data
    auto pos = search(data.begin() + start, data.end(), delimiter.begin(), delimiter.end());

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
vector<char> format_folder_contents(vector<vector<char>> &vec)
{
    vector<char> output;

    if (vec.size() == 0)
    {
        return output;
    }

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

// replaces subsrting
void replace_substring(string &str, const string &from, const string &to)
{
    size_t startPos = 0;
    while ((startPos = str.find(from, startPos)) != string::npos)
    {
        str.replace(startPos, from.length(), to);
        startPos += to.length();
    }
}

// Recursive helper function to delete folder
bool delete_folder(int fd, vector<char> parent_folder)
{
    // check parent name
    string pfolder(parent_folder.begin(), parent_folder.end());
    logger.log("parent folder: " + pfolder, LOGGER_INFO);

    // get row
    vector<char> folder_content = FeUtils::kv_get_row(fd, parent_folder);
    // content list, remove '+OK<sp>'
    std::vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
    // split on delim
    std::vector<std::vector<char>> contents = FeUtils::split_vector(folder_elements, {'\b'});

    if (!contents.empty())
    {
        logger.log("Parent folder " + string(parent_folder.begin(), parent_folder.end()) + " not empty", LOGGER_INFO);
        // Iterate through each element in the formatted contents
        for (auto col_name : contents)
        {
            if (col_name.empty())
            {
                continue;
            }
            // Check if the element is a file
            if (!is_folder(col_name))
            {
                logger.log("Found file: " + string(col_name.begin(), col_name.end()), LOGGER_INFO);
                // If it's a file, delete it
                if (kv_successful(FeUtils::kv_del(fd, parent_folder, col_name)))
                {
                    logger.log("Deleted file: " + string(col_name.begin(), col_name.end()), LOGGER_INFO);
                }
                else
                {
                    logger.log("Could not delete file: " + string(col_name.begin(), col_name.end()), LOGGER_CRITICAL);
                }
            }
            else
            {
                // If it's a folder, recursively delete its contents
                // get row of folder
                vector<char> child_folder = parent_folder;
                child_folder.insert(child_folder.end(), col_name.begin(), col_name.end());

                logger.log("Found subfolder: " + string(col_name.begin(), col_name.end()), LOGGER_INFO);
                logger.log("Subfolder path is: " + string(child_folder.begin(), child_folder.end()), LOGGER_INFO);

                delete_folder(fd, child_folder);
            }
        }
    }
    logger.log("Deleting parent folder: " + string(parent_folder.begin(), parent_folder.end()), LOGGER_INFO);
    // After deleting all contents, delete the folder itself
    if (kv_successful(FeUtils::kv_del_row(fd, parent_folder)))
    {
        logger.log("Deleted parent folder: " + string(parent_folder.begin(), parent_folder.end()), LOGGER_INFO);
        return true;
    }
    else
    {
        logger.log("Could not delete parent folder: " + string(parent_folder.begin(), parent_folder.end()), LOGGER_CRITICAL);
        return false;
    }
}

// Recursive helper function to rename folder and subfolder paths
bool move_subfolders(int fd, vector<char> parent_folder, vector<char> new_foldername, vector<char> moving_folder)
{
    logger.log("In move subfolders", LOGGER_DEBUG);
    // check parent name
    string pfolder(parent_folder.begin(), parent_folder.end());
    logger.log("parent folder: " + pfolder, LOGGER_INFO);

    // get row
    vector<char> folder_content = FeUtils::kv_get_row(fd, parent_folder);
    // content list, remove '+OK<sp>'
    std::vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
    // split on delim
    std::vector<std::vector<char>> contents = FeUtils::split_vector(folder_elements, {'\b'});

    if (!contents.empty())
    {
        logger.log("Parent folder " + string(parent_folder.begin(), parent_folder.end()) + " not empty", LOGGER_INFO);
        // Iterate through each element in the formatted contents
        for (auto col_name : contents)
        {
            if (col_name.empty())
            {
                continue;
            }
            if (is_folder(col_name))
            {
                // If it's a folder, recursively delete its contents
                // get row of folder
                vector<char> new_rowname = new_foldername;
                new_rowname.insert(new_rowname.end(), col_name.begin(), col_name.end());

                vector<char> old_rowname = parent_folder;
                old_rowname.insert(old_rowname.end(), col_name.begin(), col_name.end());

                logger.log("Old row name is " + string(old_rowname.begin(), old_rowname.end()), LOGGER_DEBUG);
                logger.log("New row name is " + string(new_rowname.begin(), new_rowname.end()), LOGGER_DEBUG);

                // the new row name should be new folder name + col : ie : user/newname/column/

                move_subfolders(fd, old_rowname, new_rowname, col_name);
            }
        }
    }

    vector<char> new_rowname = new_foldername;
    new_rowname.insert(new_rowname.end(), moving_folder.begin(), moving_folder.end());
    string new_parent_str(new_rowname.begin(), new_rowname.end());

    logger.log("Renaming parent folder: " + string(parent_folder.begin(), parent_folder.end()) + " to " + new_parent_str, LOGGER_INFO);
    // After deleting all contents, delete the folder itself
    if (kv_successful(FeUtils::kv_rename_row(fd, parent_folder, new_rowname)))
    {
        logger.log("Renamed parent folder " + pfolder + " to: " + new_parent_str, LOGGER_INFO);
        return true;
    }
    else
    {
        logger.log("Could not rename parent folder: " + string(parent_folder.begin(), parent_folder.end()), LOGGER_CRITICAL);
        return false;
    }
}

// Recursive helper function to rename folder and subfolder paths
bool rename_subfolders(int fd, vector<char> parent_folder, vector<char> new_foldername)
{
    // check parent name
    string pfolder(parent_folder.begin(), parent_folder.end());
    logger.log("parent folder: " + pfolder, LOGGER_INFO);

    // get row
    vector<char> folder_content = FeUtils::kv_get_row(fd, parent_folder);
    // content list, remove '+OK<sp>'
    std::vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
    // split on delim
    std::vector<std::vector<char>> contents = FeUtils::split_vector(folder_elements, {'\b'});

    if (!contents.empty())
    {
        logger.log("Parent folder " + string(parent_folder.begin(), parent_folder.end()) + " not empty", LOGGER_INFO);
        // Iterate through each element in the formatted contents
        for (auto col_name : contents)
        {
            if (col_name.empty())
            {
                continue;
            }
            if (is_folder(col_name))
            {
                // If it's a folder, recursively delete its contents
                // get row of folder
                vector<char> new_rowname = new_foldername;
                new_rowname.insert(new_rowname.end(), col_name.begin(), col_name.end());

                vector<char> old_rowname = parent_folder;
                old_rowname.insert(old_rowname.end(), col_name.begin(), col_name.end());

                logger.log("Old row name is " + string(old_rowname.begin(), old_rowname.end()), LOGGER_DEBUG);
                logger.log("New row name is " + string(new_rowname.begin(), new_rowname.end()), LOGGER_DEBUG);

                // the new row name should be new folder name + col : ie : user/newname/column/

                rename_subfolders(fd, old_rowname, new_rowname);
            }
        }
    }
    logger.log("Renaming parent folder: " + string(parent_folder.begin(), parent_folder.end()), LOGGER_INFO);
    // After deleting all contents, delete the folder itself
    if (kv_successful(FeUtils::kv_rename_row(fd, parent_folder, new_foldername)))
    {
        logger.log("Renamed parent folder " + pfolder + " to: " + string(new_foldername.begin(), new_foldername.end()), LOGGER_INFO);
        return true;
    }
    else
    {
        logger.log("Could not delete parent folder: " + string(parent_folder.begin(), parent_folder.end()), LOGGER_CRITICAL);
        return false;
    }
}

// Handler opens a file or folder and displays html.
// This is the landing page for drive that the user cna interact with
// there are no other get requests
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

        // path is drive/:childpath where parent dir is the page that is being displayed
        string childpath_str = req.path.substr(7);
        vector<char> child_path(childpath_str.begin(), childpath_str.end());
        bool present = HttpServer::check_kvs_addr(username);
        std::vector<std::string> kvs_addr;

        // check if we know already know the KVS server address for user
        if (present)
        {
            kvs_addr = HttpServer::get_kvs_addr(username);
        }
        // otherwise get KVS server address from coordinator
        else
        {
            // query the coordinator for the KVS server address
            kvs_addr = FeUtils::query_coordinator(username);
        }

        // create socket for communication with KVS server
        int sockfd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

        // validate session id
        string valid_session_id = FeUtils::validate_session_id(sockfd, username, req);
        // if invalid, return an error?
        // @todo :: redirect to login page?
        if (valid_session_id.empty())
        {
            // for now, returning code for check on postman
            res.set_code(303);
            res.set_header("Location", "/401");
            FeUtils::expire_cookies(res, username, sid);
            close(sockfd);
            return;
        }

        // if we are looking up a folder, use get row
        if (is_folder(child_path))
        {

            vector<char> folder_content = FeUtils::kv_get_row(sockfd, child_path);

            logger.log("Folder content is=" + string(folder_content.begin(), folder_content.end()), LOGGER_DEBUG);

            if (kv_successful(folder_content))
            {
                // content list, remove '+OK<sp>'
                std::vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
                // split on delim
                std::vector<std::vector<char>> contents = FeUtils::split_vector(folder_elements, {'\b'});
                std::vector<char> formatted_content = format_folder_contents(contents);

                // folder processing
                std::string folder_contents(formatted_content.begin(), formatted_content.end());
                std::vector<std::string> folder_items = Utils::split(folder_contents, ", ");
                sort(folder_items.begin(), folder_items.end()); // sort items
                std::string folders = "[";
                std::string folder_html = "";
                size_t item_iter = 0;
                int row_count = 0;

                while (item_iter < folder_items.size())
                {
                    std::string item = folder_items[item_iter];
                    if (item.compare("sid") != 0 && item.compare("pass") != 0)
                    {
                        // start row
                        if (row_count % 9 == 0)
                        {
                            folder_html += "<div class='row mx-2 mt-2 align-items-start'>";
                        }

                        if (row_count % 9 == 0 || row_count % 9 == 3 || row_count % 9 == 6)
                        {
                            folder_html += "<div class='col-4'><div class='row align-items-start'>";
                        }

                        // html to add item to page
                        if (item.back() == '/')
                        {
                            folders += "\"" + item + "\",";

                            folder_html +=
                                "<div class='col-4 text-center text-wrap'>"
                                "<a href='" +
                                item + "' style='color: inherit;'>"
                                       "<svg xmlns='http://www.w3.org/2000/svg' width='100%' height='100%' fill='currentColor' class='bi bi-folder-fill' viewBox='0 0 16 16'>"
                                       "<path d='M9.828 3h3.982a2 2 0 0 1 1.992 2.181l-.637 7A2 2 0 0 1 13.174 14H2.825a2 2 0 0 1-1.991-1.819l-.637-7a2 2 0 0 1 .342-1.31L.5 3a2 2 0 0 1 2-2h3.672a2 2 0 0 1 1.414.586l.828.828A2 2 0 0 0 9.828 3m-8.322.12q.322-.119.684-.12h5.396l-.707-.707A1 1 0 0 0 6.172 2H2.5a1 1 0 0 0-1 .981z'/>"
                                       "</svg>"
                                       "</a>"
                                       "<p class='text-break'>"
                                       "<a class='delete' data-bs-toggle='modal' data-bs-target='#deleteModal' href='#deleteModal' data-bs-name='" +
                                item + "' data-bs-path='" + childpath_str + "'>"
                                                                            "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='1.5em' fill='currentColor' class='bi bi-x text-danger' viewBox='0 0 16 16'>"
                                                                            "<path d='M4.646 4.646a.5.5 0 0 1 .708 0L8 7.293l2.646-2.647a.5.5 0 0 1 .708.708L8.707 8l2.647 2.646a.5.5 0 0 1-.708.708L8 8.707l-2.646 2.647a.5.5 0 0 1-.708-.708L7.293 8 4.646 5.354a.5.5 0 0 1 0-.708'/>"
                                                                            "</svg>"
                                                                            "</a>" +
                                item +
                                "</p>"
                                "</div>";
                        }
                        else
                        {
                            folder_html +=
                                "<div class='col-4 text-center text-wrap'>"
                                "<a href='" +
                                item + "' target='_blank' style='color: inherit;' download>"
                                       "<svg xmlns='http://www.w3.org/2000/svg' width='100%' height='100%' fill='currentColor' class='bi bi-file-earmark-fill' viewBox='0 0 16 16'>"
                                       "<path d='M4 0h5.293A1 1 0 0 1 10 .293L13.707 4a1 1 0 0 1 .293.707V14a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V2a2 2 0 0 1 2-2m5.5 1.5v2a1 1 0 0 0 1 1h2z'/>"
                                       "</svg>"
                                       "</a>"
                                       "<p class='text-break'>"
                                       "<a class='delete' data-bs-toggle='modal' data-bs-target='#deleteModal' href='#deleteModal' data-bs-name='" +
                                item + "' data-bs-path='" + childpath_str + "'>"
                                                                            "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='1.5em' fill='currentColor' class='bi bi-x text-danger' viewBox='0 0 16 16'>"
                                                                            "<path d='M4.646 4.646a.5.5 0 0 1 .708 0L8 7.293l2.646-2.647a.5.5 0 0 1 .708.708L8.707 8l2.647 2.646a.5.5 0 0 1-.708.708L8 8.707l-2.646 2.647a.5.5 0 0 1-.708-.708L7.293 8 4.646 5.354a.5.5 0 0 1 0-.708'/>"
                                                                            "</svg>"
                                                                            "</a>" +
                                item +
                                "</p>"
                                "</div>";
                        }

                        if (row_count % 9 == 2 || row_count % 9 == 5 || row_count % 9 == (9 - 1))
                        {
                            folder_html += "</div></div>";
                        }

                        if (row_count % 9 == (9 - 1))
                        {
                            folder_html += "</div>";
                        }

                        row_count++;
                    }
                    item_iter++;
                }
                if (row_count % 9 != (9 - 1))
                    folder_html += "</div>";

                // construct array of folders in cwd
                if (row_count > 0)
                    folders.pop_back();
                folders.push_back(']');

                std::vector<std::string> path_elems = Utils::split(childpath_str, "/");
                std::string drive = "";
                for (row_count = path_elems.size() - 1; row_count >= 0; row_count--)
                {
                    if (path_elems.size() - row_count == 1)
                    {
                        drive = "<a href='./'>" + path_elems[row_count] + "</a>/" + drive;
                    }
                    else if (path_elems.size() - row_count < 4)
                    {
                        drive = "<a href='" + (path_elems.size() - row_count == 2 ? std::string("../") : std::string("../../")) + "'>" + path_elems[row_count] + "</a>/" + drive;
                    }
                    else
                    {
                        drive = "../" + drive;
                        break;
                    }
                }

                // construct html page
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
                    "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
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
                               "<div class='row mx-2 mt-3 mb-4 align-items-center'>"
                               "<div class='col-5'>"
                               "<h1 class='display-6'>"
                               "Drive: " +
                    drive +
                    "</h1>"
                    "</div>"
                    "<div class='col-1 text-center'>"
                    "<button type='button' class='btn btn-primary text-center' data-bs-toggle='modal' data-bs-target='#renameModal' disabled>"
                    "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16' fill='currentColor' class='bi bi-pen' viewBox='0 0 16 16'>"
                    "<path d='m13.498.795.149-.149a1.207 1.207 0 1 1 1.707 1.708l-.149.148a1.5 1.5 0 0 1-.059 2.059L4.854 14.854a.5.5 0 0 1-.233.131l-4 1a.5.5 0 0 1-.606-.606l1-4a.5.5 0 0 1 .131-.232l9.642-9.642a.5.5 0 0 0-.642.056L6.854 4.854a.5.5 0 1 1-.708-.708L9.44.854A1.5 1.5 0 0 1 11.5.796a1.5 1.5 0 0 1 1.998-.001m-.644.766a.5.5 0 0 0-.707 0L1.95 11.756l-.764 3.057 3.057-.764L14.44 3.854a.5.5 0 0 0 0-.708z'/>"
                    "</svg><br/>"
                    "Rename Item"
                    "</button>"
                    "</div>"
                    "<div class='col-1 text-center'>"
                    "<button type='button' class='btn btn-primary text-center' data-bs-toggle='modal' data-bs-target='#moveModal' disabled>"
                    "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16' fill='currentColor' class='bi bi-arrows-move' viewBox='0 0 16 16'>"
                    "<path fill-rule='evenodd' d='M7.646.146a.5.5 0 0 1 .708 0l2 2a.5.5 0 0 1-.708.708L8.5 1.707V5.5a.5.5 0 0 1-1 0V1.707L6.354 2.854a.5.5 0 1 1-.708-.708zM8 10a.5.5 0 0 1 .5.5v3.793l1.146-1.147a.5.5 0 0 1 .708.708l-2 2a.5.5 0 0 1-.708 0l-2-2a.5.5 0 0 1 .708-.708L7.5 14.293V10.5A.5.5 0 0 1 8 10M.146 8.354a.5.5 0 0 1 0-.708l2-2a.5.5 0 1 1 .708.708L1.707 7.5H5.5a.5.5 0 0 1 0 1H1.707l1.147 1.146a.5.5 0 0 1-.708.708zM10 8a.5.5 0 0 1 .5-.5h3.793l-1.147-1.146a.5.5 0 0 1 .708-.708l2 2a.5.5 0 0 1 0 .708l-2 2a.5.5 0 0 1-.708-.708L14.293 8.5H10.5A.5.5 0 0 1 10 8'/>"
                    "</svg><br/>"
                    "Move Item"
                    "</button>"
                    "</div>"
                    "<div class='col-1 text-center'>"
                    "<button type='button' class='btn btn-primary text-center' data-bs-toggle='modal' data-bs-target='#createFolderModal'>"
                    "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16' fill='currentColor' class='bi bi-folder-fill' viewBox='0 0 16 16'>"
                    "<path d='M9.828 3h3.982a2 2 0 0 1 1.992 2.181l-.637 7A2 2 0 0 1 13.174 14H2.825a2 2 0 0 1-1.991-1.819l-.637-7a2 2 0 0 1 .342-1.31L.5 3a2 2 0 0 1 2-2h3.672a2 2 0 0 1 1.414.586l.828.828A2 2 0 0 0 9.828 3m-8.322.12q.322-.119.684-.12h5.396l-.707-.707A1 1 0 0 0 6.172 2H2.5a1 1 0 0 0-1 .981z'></path>"
                    "</svg><br/>"
                    "Create Folder"
                    "</button>"
                    "</div>"
                    "<div class='col-4'>"
                    "<form class='d-flex' role='upload' method='POST' enctype='multipart/form-data' action='/api/drive/upload/" +
                    childpath_str + "'>"
                                    "<input class='form-control' style='height: 80%; width: auto;' type='file' id='formFile' name='file'>"
                                    "<input class='btn btn-link link-underline link-underline-opacity-0' type='submit' value='Upload' disabled>"
                                    "</form>" // place upload button here
                                    "<script>"
                                    "$('input:file').on('change', function() {"
                                    "$('input:submit').prop('disabled', !$(this).val());"
                                    "});"
                                    "</script>"
                                    "</div>"
                                    "</div>" +
                    folder_html +
                    "</div>"

                    "<div class='modal fade' id='deleteModal' tabindex='-1' aria-labelledby='deleteModalLabel' aria-hidden='true'>"
                    "<div class='modal-dialog modal-dialog-centered'>"
                    "<div class='modal-content'>"
                    "<div class='modal-header'>"
                    "<h1 class='modal-title fs-5' id='deleteModalLabel'>Are you sure you want to delete </h1>"
                    "<button type='button' class='btn-close' data-bs-dismiss='modal' aria-label='Close'></button>"
                    "</div>"
                    "<div class='modal-body'>"
                    "<p>Deleting this item is an unrecoverable action and will result in this item being deleted from storage permanently!</p>"
                    "<form id='deleteForm' method='POST'>"
                    "</form>"
                    "</div>"
                    "<div class='modal-footer'>"
                    "<button type='button' class='btn btn-secondary' data-bs-dismiss='modal'>Close</button>"
                    "<button type='submit' class='btn btn-danger' onclick='$(\"#deleteForm\").submit();'>Delete</button>"
                    "</div>"
                    "</div>"
                    "</div>"
                    "</div>"

                    "<div class='modal fade' id='createFolderModal' tabindex='-1' aria-labelledby='createFolderModalLabel' aria-hidden='true'>"
                    "<div class='modal-dialog modal-dialog-centered'>"
                    "<div class='modal-content'>"
                    "<div class='modal-header'>"
                    "<h1 class='modal-title fs-5' id='createFolderModalLabel'>Create a new folder</h1>"
                    "<button type='button' class='btn-close' data-bs-dismiss='modal' aria-label='Close'></button>"
                    "</div>"
                    "<div class='modal-body'>"
                    "<form id='createFolderForm' method='POST' action='/api/drive/create/" +
                    childpath_str + "'>"
                                    "<div class='mb-3'>"
                                    "<label for='folder-name' class='col-form-label'>Folder name:</label>"
                                    "<input name='name' type='text' class='form-control' id='folder-name' minlength=1 maxlength=255 pattern='^[\\w\\-]+$' required placeholder='My_Folder-27' aria-describedby='folderHelp' oninput='setCustomValidity(\"\")'>"
                                    "<div id='folderHelp' class='form-text'>Names can contain letters, numbers, hyphens, and underscores</div>"
                                    "</div>"
                                    "</form>"
                                    "</div>"
                                    "<div class='modal-footer'>"
                                    "<button type='button' class='btn btn-secondary' data-bs-dismiss='modal'>Close</button>"
                                    "<button type='submit' class='btn btn-primary' onclick='var folders = " +
                    folders + "; if ($(\"#createFolderForm\")[0].checkValidity()) {if (folders.includes($(\"#createFolderForm\")[0][0].value + \"/\")) { $(\"#createFolderForm\")[0][0].setCustomValidity(\"Folder already exists.\"); $(\"#createFolderForm\")[0].reportValidity(); } else {$(\"#createFolderForm\").submit();} } else {$(\"#createFolderForm\")[0][0].setCustomValidity(($(\"#createFolderForm\")[0][0].value.length !== 0 ? \"Some of the input characters are bad.\" : \"Please fill out this field.\")); $(\"#createFolderForm\")[0].reportValidity();}'>Create</button>"
                              "</div>"
                              "</div>"
                              "</div>"
                              "</div>"

                              "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
                              "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
                              "crossorigin='anonymous'></script>"
                              "<script>"
                              "$('.delete').on('click', function() {"
                              "    $('#deleteModal').modal('show');"
                              "});"

                              "$('#deleteModal').on('show.bs.modal', function(e) {"
                              "let item_name = $(e.relatedTarget).attr('data-bs-name');"
                              "let file_path = $(e.relatedTarget).attr('data-bs-path');"
                              "$('#deleteModalLabel').html('Are you sure you want to delete ' + item_name + '?');"
                              "$('#deleteForm').attr('action', '/api/drive/delete/' + file_path + item_name);"
                              "});"
                              "</script>"
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
                res.append_body_str(page);
                res.set_code(200);
                res.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
                res.set_header("Pragma", "no-cache");
                res.set_header("Expires", "0");
                FeUtils::set_cookies(res, username, sid);
            }
            else
            {
                // @PETER ADDED
                // set response status code
                res.set_code(303);
                // set response headers / redirect to 400 error
                logger.log("603 setting 400", LOGGER_DEBUG);
                res.set_header("Location", "/400");
                FeUtils::expire_cookies(res, username, sid);
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
                FeUtils::set_cookies(res, username, sid);

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
                logger.log("647 setting 400", LOGGER_DEBUG);
                res.set_header("Location", "/400");
                FeUtils::expire_cookies(res, username, sid);
            }
        }

        close(sockfd);
    }
    else
    {
        // set response status code
        res.set_code(303);

        // set response headers / redirect to 401 error
        res.set_header("Location", "/401");
    }
}

void upload_file(const HttpRequest &req, HttpResponse &res)
{

    // Get path of parent directory where we are appending

    // path is /api/drive/upload/:parentpath where parent dir is the page that is being displayed
    string parentpath_str = req.path.substr(18);
    string username = get_username(parentpath_str);
    bool present = HttpServer::check_kvs_addr(username);
    std::vector<std::string> kvs_addr;

    // check if we know already know the KVS server address for user
    if (present)
    {
        kvs_addr = HttpServer::get_kvs_addr(username);
    }
    // otherwise get KVS server address from coordinator
    else
    {
        // query the coordinator for the KVS server address
        kvs_addr = FeUtils::query_coordinator(username);
    }

    // create socket for communication with KVS server
    int sockfd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

    // validate session id
    string valid_session_id = FeUtils::validate_session_id(sockfd, username, req);
    // if invalid, return an error?
    // @todo :: redirect to login page?
    if (valid_session_id.empty())
    {
        // for now, returning code for check on postman

        res.set_code(303);
        res.set_header("Location", "/401");

        close(sockfd);
        return;
    }

    // Check if the request contains a body
    if (!req.body_as_bytes().empty())
    {
        // ------- Get file name and clean up to get file binary ----
        // Get the single file uploaded
        vector<string> headers;

        // Find form boundary
        headers = req.get_header("Content-Type");
        string header_str(headers[0]);
        // boundary provided by form
        vector<string> find_boundary = Utils::split_on_first_delim(header_str, "boundary=");
        vector<char> bound_vec(find_boundary.back().begin(), find_boundary.back().end());

        vector<char> line_feed = {'\r', '\n'};

        // split body on boundary
        vector<vector<char>> elements = FeUtils::split_vector(req.body_as_bytes(), bound_vec);
        vector<char> elem1 = elements[1];

        // @note: assuming we only upload 1 file at a time?
        vector<char> file_data = split_vec_first_delim(elem1, line_feed)[1];

        // split file data to separate file metadata from binary values
        vector<vector<char>> body_elems = split_vec_first_delim(file_data, line_feed);

        // parse file headers to get name of file
        string file_headers(body_elems[0].begin(), body_elems[0].end());
        string content_disp = Utils::split_on_first_delim(file_headers, "\n")[0];
        string filename_toparse = Utils::split_on_first_delim(file_headers, "filename=").back();

        // file name string
        string filename = Utils::split(filename_toparse, "\"")[0];

        // get file binary
        vector<char> file_binary = split_vec_first_delim(body_elems[1], line_feed)[1];
        file_binary = split_vec_first_delim(file_binary, line_feed)[1];

        if (parentpath_str.back() != '/')
        {
            res.set_code(303);
            // set cookies on response
            logger.log("748 setting 400", LOGGER_DEBUG);
            res.set_header("Location", "/400");
            FeUtils::expire_cookies(res, username, valid_session_id);
            close(sockfd);
            return;
        }
        string childpath_str = parentpath_str + filename;

        vector<char> row_vec(parentpath_str.begin(), parentpath_str.end());
        vector<char> col_vec(filename.begin(), filename.end());

        vector<char> kvs_resp = FeUtils::kv_put(sockfd, row_vec, col_vec, file_binary);

        if (kv_successful(kvs_resp))
        {
            // @todo should we instead get row for the page they are on?
            res.set_code(303); // OK
            res.set_header("Location", "/drive/" + parentpath_str);
            // vector<char> folder_content = FeUtils::kv_get_row(sockfd, row_vec);
        }
        else
        {
            res.set_code(303); // Bad Request
            logger.log("771 setting 400", LOGGER_DEBUG);
            res.set_header("Location", "/400");
            FeUtils::expire_cookies(res, username, valid_session_id);
            // maybe retry? tbd
        }
    }
    else
    {
        // No body found in the request
        res.set_code(303); // Bad Request
        logger.log("781 setting 400", LOGGER_DEBUG);
        res.set_header("Location", "/400");
    }

    // set cookies on response
    FeUtils::set_cookies(res, username, valid_session_id);

    close(sockfd);
}

// creates a new folder
void create_folder(const HttpRequest &req, HttpResponse &res)
{
    // uses a post request to add a new folder to the current parent directory.

    // path is /api/drive/create/:parentpath where parent dir is the page that is being displayed
    string parentpath_str = req.path.substr(18);

    string username = get_username(parentpath_str);
    bool present = HttpServer::check_kvs_addr(username);
    std::vector<std::string> kvs_addr;

    // check if we know already know the KVS server address for user
    if (present)
    {
        kvs_addr = HttpServer::get_kvs_addr(username);
    }
    // otherwise get KVS server address from coordinator
    else
    {
        // query the coordinator for the KVS server address
        kvs_addr = FeUtils::query_coordinator(username);
    }

    // create socket for communication with KVS server
    int sockfd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

    // validate session id
    string valid_session_id = FeUtils::validate_session_id(sockfd, username, req);
    // if invalid, return an error?
    // @todo :: redirect to login page?
    if (valid_session_id.empty())
    {
        // for now, returning code for check on postman
        res.set_code(303);
        res.set_header("Location", "/401");
        // res.set_code(401);
        close(sockfd);
        return;
    }

    string req_body = req.body_as_string();

    // check that ody is not empty
    if (!req_body.empty())
    {
        // get name of folder
        string key = "name=";
        vector<string> elements = Utils::split_on_first_delim(req_body, key);

        // if key doesn't exist, return 400
        if (elements.size() < 1)
        {
            res.set_code(303);
            logger.log("845 setting 400", LOGGER_DEBUG);
            res.set_header("Location", "/400");
            // res.set_code(400);
            return;
        }

        vector<char> folder_name(elements[0].begin(), elements[0].end());
        folder_name.push_back('/');
        vector<char> row_name(parentpath_str.begin(), parentpath_str.end());

        vector<char> folder_content = FeUtils::kv_get_row(sockfd, row_name);

        // content list, remove '+OK<sp>'
        vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
        // split on delim
        vector<vector<char>> contents = FeUtils::split_vector(folder_elements, {'\b'});
        vector<char> formatted_content = format_folder_contents(contents);

        // if folder name in use - @PETER add a front-end validation check for this
        if (contains_subseq(formatted_content, folder_name))
        {
            // currently returning 400 but not sure what behavior should be
            res.set_code(303);
            logger.log("868 setting 400", LOGGER_DEBUG);
            res.set_header("Location", "/400");
            // res.set_code(400);
        }
        else
        {
            logger.log("trying to create folder " + elements[0], LOGGER_DEBUG);

            vector<char> response = FeUtils::kv_put(sockfd, row_name, folder_name, {});

            logger.log("kvs response: " + string(response.begin(), response.end()), LOGGER_DEBUG);

            if (kv_successful(response))
            {

                logger.log("create folder " + elements[0] + " success", LOGGER_DEBUG);

                // create new column for row
                vector<char> folder_row = row_name;
                folder_row.insert(folder_row.end(), folder_name.begin(), folder_name.end());
                vector<char> kvs_resp = FeUtils::kv_put(sockfd, folder_row, {}, {});

                // get parent folder to show that this folder has been nested
                // folder_content = FeUtils::kv_get_row(sockfd, row_name);

                // content list, remove '+OK<sp>'
                // vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
                // contents = FeUtils::split_vector(folder_elements, {'\b'});
                // formatted_content = format_folder_contents(contents);
                // res.append_body_bytes(formatted_content.data(), formatted_content.size());
                // res.set_code(200);

                res.set_code(303);
                res.set_header("Location", "/drive/" + std::string(row_name.begin(), row_name.end()));

                // set cookies on response
                FeUtils::set_cookies(res, username, valid_session_id);
            }
            else
            {

                logger.log("create folder " + elements[0] + " failed", LOGGER_DEBUG);
                logger.log("894 setting 400", LOGGER_DEBUG);
                res.set_code(303);
                res.set_header("Location", "/400");
                // res.set_code(400);
            }
        }
    }
    else
    {
        logger.log("903 setting 400", LOGGER_DEBUG);
        res.set_code(303);
        res.set_header("Location", "/400");
    }

    close(sockfd);
}

// deletes file or folder
// @todo is this a post or a get? I think post with no body?
void delete_filefolder(const HttpRequest &req, HttpResponse &res)
{
    logger.log("In delete filefolder", LOGGER_DEBUG);
    // of type /api/drive/delete/* where child directory is being served
    string childpath_str = req.path.substr(18);
    string username = get_username(childpath_str);
    bool present = HttpServer::check_kvs_addr(username);
    std::vector<std::string> kvs_addr;

    // check if we know already know the KVS server address for user
    if (present)
    {
        kvs_addr = HttpServer::get_kvs_addr(username);
    }
    // otherwise get KVS server address from coordinator
    else
    {
        // query the coordinator for the KVS server address
        kvs_addr = FeUtils::query_coordinator(username);
    }

    // create socket for communication with KVS server
    int sockfd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

    // validate session id
    string valid_session_id = FeUtils::validate_session_id(sockfd, username, req);
    // if invalid, return an error?
    // @todo :: redirect to login page?
    if (valid_session_id.empty())
    {
        // for now, returning code for check on postman
        res.set_code(401);
        close(sockfd);
        return;
    }

    vector<char> child_path(childpath_str.begin(), childpath_str.end());

    // if we are trying to delete a file
    if (!is_folder(child_path))
    {
        // get file name
        string filename;
        string parentpath_str = split_parent_filename(Utils::split(childpath_str, "/"), filename);

        // comver tto vector<char>
        vector<char> parent_path_vec(parentpath_str.begin(), parentpath_str.end());
        vector<char> filename_vec(filename.begin(), filename.end());

        if (kv_successful(FeUtils::kv_del(sockfd, parent_path_vec, filename_vec)))
        {

            res.set_code(303);
            res.set_header("Location", "/drive/" + parentpath_str);
        }
        else
        {
            logger.log("970 setting 400", LOGGER_DEBUG);
            res.set_code(303);
            res.set_header("Location", "/400");
        }
    }
    else
    {
        logger.log("Identified delete folder", LOGGER_DEBUG);

        // get row
        vector<char> folder_content = FeUtils::kv_get_row(sockfd, child_path);

        logger.log("Child path is " + childpath_str, LOGGER_DEBUG);

        if (kv_successful(folder_content))
        {
            // recursively delete the folders children
            if (delete_folder(sockfd, child_path))
            {

                // delete folder from parent
                // get parent path
                // get folder name
                string foldername;

                logger.log("Child path is " + childpath_str, LOGGER_DEBUG);
                vector<string> split_filepath = Utils::split(childpath_str, "/");
                string parentpath_str = split_parent_filename(split_filepath, foldername);

                foldername += '/';

                logger.log("Parent path is " + parentpath_str, LOGGER_DEBUG);
                logger.log("Folder name is " + foldername, LOGGER_DEBUG);

                vector<char> parent_path_vec(parentpath_str.begin(), parentpath_str.end());
                vector<char> folder_name_vec(foldername.begin(), foldername.end());

                logger.log("Deleting folder from parent:" + foldername, LOGGER_DEBUG);

                // dleete the folder from the parent folder
                if (kv_successful(FeUtils::kv_del(sockfd, parent_path_vec, folder_name_vec)))
                {
                    logger.log("Deleted folder from parent:" + foldername, LOGGER_DEBUG);

                    // redirect
                    res.set_code(303);
                    res.set_header("Location", "/drive/" + parentpath_str);
                }
                else
                {
                    logger.log("Could not delete folder from parent:" + foldername, LOGGER_CRITICAL);
                    // redirect
                    res.set_code(303);
                    res.set_header("Location", "/400");
                }
            }
        }
        else
        {

            logger.log("Retreiving folder " + childpath_str + " failed.", LOGGER_WARN);
            res.set_code(303);
            res.set_header("Location", "/400");
        }
    }

    // set cookies on response
    FeUtils::set_cookies(res, username, valid_session_id);

    close(sockfd);
}

// renames file or folder
// post with form attribtue
void rename_filefolder(const HttpRequest &req, HttpResponse &res)
{

    logger.log("In rename filefolder", LOGGER_DEBUG);
    // of type /api/drive/rename/* where child directory is being served
    string childpath_str = req.path.substr(18);
    string username = get_username(childpath_str);
    bool present = HttpServer::check_kvs_addr(username);
    std::vector<std::string> kvs_addr;

    // check if we know already know the KVS server address for user
    if (present)
    {
        kvs_addr = HttpServer::get_kvs_addr(username);
    }
    // otherwise get KVS server address from coordinator
    else
    {
        // query the coordinator for the KVS server address
        kvs_addr = FeUtils::query_coordinator(username);
    }

    // create socket for communication with KVS server
    int sockfd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

    // validate session id
    string valid_session_id = FeUtils::validate_session_id(sockfd, username, req);
    // if invalid, return an error?
    // @todo :: redirect to login page?
    if (valid_session_id.empty())
    {
        // for now, returning code for check on postman
        res.set_code(401);
        close(sockfd);
        return;
    }

    vector<char> child_path(childpath_str.begin(), childpath_str.end());

    // get new name using parameters
    // Extract form parameters (status and component id) from the HTTP request body
    std::string requestBody = req.body_as_string();
    std::unordered_map<std::string, std::string> formParams;

    // Parse the request body to extract form parameters
    size_t pos = 0;
    while ((pos = requestBody.find('&')) != std::string::npos)
    {
        std::string token = requestBody.substr(0, pos);
        size_t equalPos = token.find('=');
        std::string key = token.substr(0, equalPos);
        std::string value = token.substr(equalPos + 1);
        formParams[key] = value;
        requestBody.erase(0, pos + 1);
    }
    // Handle the last parameter
    size_t equalPos = requestBody.find('=');
    std::string key = requestBody.substr(0, equalPos);
    std::string value = requestBody.substr(equalPos + 1);
    formParams[key] = value;

    // get new name
    string newname = formParams["name"];
    vector<char> newname_vec(newname.begin(), newname.end());

    logger.log("New name is:" + newname, LOGGER_DEBUG);

    // if we are trying to rename a file
    if (!is_folder(child_path))
    {
        // get file name
        string filename;
        string parentpath_str = split_parent_filename(Utils::split(childpath_str, "/"), filename);

        // comver tto vector<char>
        vector<char> parent_path_vec(parentpath_str.begin(), parentpath_str.end());
        vector<char> filename_vec(filename.begin(), filename.end());

        if (kv_successful(FeUtils::kv_rename_col(sockfd, parent_path_vec, filename_vec, newname_vec)))
        {

            res.set_code(303);
            res.set_header("Location", "/drive/" + parentpath_str);
        }
        else
        {
            logger.log("970 setting 400", LOGGER_DEBUG);
            res.set_code(303);
            res.set_header("Location", "/400");
        }
    }
    else
    {
        logger.log("Identified rename folder", LOGGER_DEBUG);

        // get row
        vector<char> folder_content = FeUtils::kv_get_row(sockfd, child_path);

        logger.log("Child path is " + childpath_str, LOGGER_DEBUG);

        if (kv_successful(folder_content))
        {

            newname += '/';

            // delete folder from parent
            // get parent path
            // get folder name
            string foldername;

            logger.log("Child path is " + childpath_str, LOGGER_DEBUG);
            vector<string> split_filepath = Utils::split(childpath_str, "/");
            string parentpath_str = split_parent_filename(split_filepath, foldername);

            foldername += '/';

            logger.log("Parent path is " + parentpath_str, LOGGER_DEBUG);
            logger.log("Folder name is " + foldername, LOGGER_DEBUG);

            vector<char> parent_path_vec(parentpath_str.begin(), parentpath_str.end());
            vector<char> folder_name_vec(foldername.begin(), foldername.end());

            newname_vec.push_back('/');

            vector<char> new_folderpath = parent_path_vec;
            new_folderpath.insert(new_folderpath.end(), newname_vec.begin(), newname_vec.end());

            logger.log("New child path is: " + string(new_folderpath.begin(), new_folderpath.end()), LOGGER_DEBUG);

            // recursively delete the folders children
            if (rename_subfolders(sockfd, child_path, new_folderpath))
            {
                logger.log("Rename recursion success", LOGGER_DEBUG);
                logger.log("Renaming folder'" + foldername + "' to " + newname, LOGGER_DEBUG);
                // dleete the folder from the parent folder
                if (kv_successful(FeUtils::kv_rename_col(sockfd, parent_path_vec, folder_name_vec, newname_vec)))
                {
                    logger.log("renamed folder'" + foldername + "' to " + newname, LOGGER_DEBUG);

                    // redirect
                    res.set_code(303);
                    res.set_header("Location", "/drive/" + parentpath_str);
                }
                else
                {
                    logger.log("Could not delete folder from parent:" + foldername, LOGGER_CRITICAL);
                    // redirect
                    res.set_code(303);
                    res.set_header("Location", "/400");
                }
            }
        }
        else
        {

            logger.log("Retreiving folder " + childpath_str + " failed.", LOGGER_WARN);
            res.set_code(303);
            res.set_header("Location", "/400");
        }
    }

    // set cookies on response
    FeUtils::set_cookies(res, username, valid_session_id);

    close(sockfd);
}

// Moves file or folder to new location
// post with form attribtue newparent
void move_filefolder(const HttpRequest &req, HttpResponse &res)
{

    logger.log("In rename filefolder", LOGGER_DEBUG);
    // of type /api/drive/move/* where child directory is being served
    string childpath_str = req.path.substr(16);
    string username = get_username(childpath_str);
    bool present = HttpServer::check_kvs_addr(username);
    std::vector<std::string> kvs_addr;

    // check if we know already know the KVS server address for user
    if (present)
    {
        kvs_addr = HttpServer::get_kvs_addr(username);
    }
    // otherwise get KVS server address from coordinator
    else
    {
        // query the coordinator for the KVS server address
        kvs_addr = FeUtils::query_coordinator(username);
    }

    // create socket for communication with KVS server
    int sockfd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

    // validate session id
    string valid_session_id = FeUtils::validate_session_id(sockfd, username, req);
    // if invalid, return an error?
    // @todo :: redirect to login page?
    if (valid_session_id.empty())
    {
        // for now, returning code for check on postman
        res.set_code(401);
        close(sockfd);
        return;
    }

    vector<char> child_path(childpath_str.begin(), childpath_str.end());

    // get new name using parameters
    // Extract form parameters (status and component id) from the HTTP request body
    std::string requestBody = req.body_as_string();
    std::unordered_map<std::string, std::string> formParams;

    // Parse the request body to extract form parameters
    size_t pos = 0;
    while ((pos = requestBody.find('&')) != std::string::npos)
    {
        std::string token = requestBody.substr(0, pos);
        size_t equalPos = token.find('=');
        std::string key = token.substr(0, equalPos);
        std::string value = token.substr(equalPos + 1);
        formParams[key] = value;
        requestBody.erase(0, pos + 1);
    }
    // Handle the last parameter
    size_t equalPos = requestBody.find('=');
    std::string key = requestBody.substr(0, equalPos);
    std::string value = requestBody.substr(equalPos + 1);
    formParams[key] = value;

    // get new name
    string newparent = formParams["newparent"];

    replace_substring(newparent, "%2F", "/");

    vector<char> newparent_vec(newparent.begin(), newparent.end());

    logger.log("New parent is:" + newparent, LOGGER_DEBUG);

    // if we are trying to move a filde
    if (!is_folder(child_path))
    {
        // get file name
        string filename;
        string parentpath_str = split_parent_filename(Utils::split(childpath_str, "/"), filename);

        // comver tto vector<char>
        vector<char> parent_path_vec(parentpath_str.begin(), parentpath_str.end());
        vector<char> filename_vec(filename.begin(), filename.end());

        // get file content
        vector<char> file_content = FeUtils::kv_get(sockfd, parent_path_vec, filename_vec);

        // if error, return
        if (!kv_successful(file_content))
        {
            logger.log("1378 setting 400", LOGGER_DEBUG);
            res.set_code(303);
            res.set_header("Location", "/400");
            return;
        }
        // get binary from 4th char onward (ignore +OK<sp>)
        std::vector<char> file_binary(file_content.begin() + 4, file_content.end());

        // delete file from old parent
        if (!kv_successful(FeUtils::kv_del(sockfd, parent_path_vec, filename_vec)))
        {
            logger.log("Could not delete file " + filename + " from old parent " + parentpath_str, LOGGER_WARN);
            res.set_code(303);
            res.set_header("Location", "/400");

            // set cookies on response
            FeUtils::set_cookies(res, username, valid_session_id);

            close(sockfd);
            return;
        }

        // put file into new parent
        if (!kv_successful(FeUtils::kv_put(sockfd, newparent_vec, filename_vec, file_binary)))
        {
            logger.log("Could not add file " + filename + " to new parent " + newparent, LOGGER_WARN);
            res.set_code(303);
            res.set_header("Location", "/400");

            // set cookies on response
            FeUtils::set_cookies(res, username, valid_session_id);

            close(sockfd);
            return;
        }
        else
        {
            logger.log("File " + filename + " successfully moved to new parent " + newparent, LOGGER_INFO);
            // redirect
            // @todo: redirect to new parent or current?
            res.set_code(303);
            res.set_header("Location", "/drive/" + parentpath_str);
        }
    }
    else
    {
        logger.log("Identified move folder", LOGGER_DEBUG);

        // get row
        vector<char> folder_content = FeUtils::kv_get_row(sockfd, child_path);

        logger.log("Child path is " + childpath_str, LOGGER_DEBUG);

        if (kv_successful(folder_content))
        {

            // move folder from parent
            // get parent path
            // get folder name
            string foldername;

            logger.log("Child path is " + childpath_str, LOGGER_DEBUG);
            vector<string> split_filepath = Utils::split(childpath_str, "/");
            string parentpath_str = split_parent_filename(split_filepath, foldername);

            foldername += '/';

            logger.log("Parent path is " + parentpath_str, LOGGER_DEBUG);
            logger.log("Folder name is " + foldername, LOGGER_DEBUG);

            vector<char> parent_path_vec(parentpath_str.begin(), parentpath_str.end());
            vector<char> folder_name_vec(foldername.begin(), foldername.end());

            logger.log("Moving folder to new parent by renaming subfolders", LOGGER_DEBUG);

            // recursively delete the folders children
            if (move_subfolders(sockfd, child_path, newparent_vec, folder_name_vec))
            {
                logger.log("Rename recursion success", LOGGER_DEBUG);

                // Delete folder from old parent
                if (!kv_successful(FeUtils::kv_del(sockfd, parent_path_vec, folder_name_vec)))
                {
                    logger.log("Could not delete file " + foldername + " from old parent " + parentpath_str, LOGGER_WARN);
                    res.set_code(303);
                    res.set_header("Location", "/400");
                    return;
                }

                // put file into new parent
                if (!kv_successful(FeUtils::kv_put(sockfd, newparent_vec, folder_name_vec, {})))
                {
                    logger.log("Could not add file " + foldername + " to new parent " + newparent, LOGGER_WARN);
                    res.set_code(303);
                    res.set_header("Location", "/400");
                    // set cookies on response
                    FeUtils::set_cookies(res, username, valid_session_id);

                    close(sockfd);
                    return;
                }
                else
                {
                    logger.log("Folder " + foldername + " successfully moved to new parent " + newparent, LOGGER_INFO);
                    // redirect
                    // @todo: redirect to new parent or current?
                    res.set_code(303);
                    res.set_header("Location", "/drive/" + parentpath_str);
                }
            }
        }
        else
        {

            logger.log("Retreiving folder " + childpath_str + " failed.", LOGGER_WARN);
            res.set_code(303);
            res.set_header("Location", "/400");
        }
    }

    // set cookies on response
    FeUtils::set_cookies(res, username, valid_session_id);

    close(sockfd);
}