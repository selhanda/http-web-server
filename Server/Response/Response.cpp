#include "Response.hpp"
#include "../Request/Request.hpp"

Response::Response(std::string Path, std::string method, std::string contentType, int new_socket,
 bool is_cgi, std::vector<std::string> indexs,
  bool autoindex, std::string full_path, std::string req_path, bool is_location , std::string cookies_part, std::map<std::string, std::string> error_pages)
{
    this->socket_fd = new_socket;
    this->is_header_send = false;
    this->index_files = indexs;
    this->auto_index = autoindex;
    this->full_path = full_path;
    this->req_path = req_path;
    this->is_location = is_location;
    this->server_cookies = cookies_part;
    this->error_pages = error_pages;
    (void)is_cgi;
    std::string file_content;


    if (method == "No")
    {
        this->res_to_client = "HTTP/1.1 405 Method Not Allowed\r\nContent-type: text/html\r\n" + this->server_cookies  +"\r\n";
        file_content = read_file_content(this->error_pages["405"]);
        this->res_to_client += file_content;
        this->num_sent = send(this->socket_fd, this->res_to_client.data(), this->res_to_client.length(), 0);
    }

    else if (method == "GET")
    {
        this->num_sent = handle_get_request(Path, contentType);
    }
    else if (method == "POST")
    {
        this->num_sent = handle_get_request(Path, contentType);
    }
    else if (method == "DELETE")
        this->num_sent = handel_delete_request(Path);
}

std::string Response::read_file_content(std::string Path)
{
    std::ifstream file(Path.substr(1), std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string file_contents = buffer.str();
    return (file_contents);
}

std::string Response::check_request_path(std::string Path)
{
    std::string header;
    std::ifstream file(Path.substr(1), std::ios::binary);
    if (file)
    {
        this->Status = 200;
        header = "HTTP/1.1 200 OK\r\nContent-type: ";
    }
    else
    {
        this->Status = 404;
        header = "HTTP/1.1 404 Not Found\r\nContent-type: ";
    }
    return (header);
}