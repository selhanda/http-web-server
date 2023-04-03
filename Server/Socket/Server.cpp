#include "Server.hpp"

Server::Server(Config config)
{
    path_check = 0;
    connection(config.Servers);
}


void Server::connection(std::vector<ServerBlock> &servers)
{
    this->sockets = setup_sockets(servers);
    std::vector<pollfd> pollfds = create_pollfds(servers);

    int tmp = 0;
    while(1)
    {
        int ready_count = poll(&pollfds[0], pollfds.size(), -1);
        if (ready_count == -1) {
            std::cout << "Error in poll" << std::endl;
            exit(1);
        }
        for (size_t i = 0; i < pollfds.size(); i++) {
            
            if (pollfds[i].revents & POLLIN) {
                if (i < servers.size())
                {
                    if (pollfds[i].fd == servers[i].sock_fd) 
                    {
                        handle_new_connection(servers[i].sock_fd, pollfds);
                        tmp = i;
                    }
                }
                else 
                {
                    try 
                    {
                        respond_to_clients(pollfds[i].fd, root_paths[tmp], servers[tmp], tmp);
                    }
                    catch(const std::exception &e)
                    {
                        continue;
                    }
                }
            }
        }
    }
}



std::vector<int> Server::setup_sockets(std::vector<ServerBlock> &servers)
{
    std::vector<int> ret_sockets;

    for (size_t i =0; i < servers.size(); i++)
    {
        struct sockaddr_in address;
        int option = 1;
        servers[i].sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (servers[i].sock_fd == 0)
        {
            std::cout << "error in the socket" << std::endl;
            exit(1);
        }
        setsockopt(servers[i].sock_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
        
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(servers[i].port);

        if (bind(servers[i].sock_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            std::cout << "error in binding" << std::endl;
            exit(1);
        }


        if (listen(servers[i].sock_fd, 10) < 0)
        {
            std::cout << "error in listening" << std::endl;
            exit(1);  
        }
        set_nonblocking(servers[i].sock_fd);
        ret_sockets.push_back(servers[i].sock_fd);
        root_paths.push_back(servers[i].root);
        std::cout << "listening on port => " << servers[i].port << std::endl;

    }
    return (ret_sockets);
}


void Server::respond_to_clients(int client_socket, std::string root_path, ServerBlock server, int tmp)
{
    std::string request_message;
    int bytes_received;
    char buffer[1024];
    std::string full_path;

    set_nonblocking(client_socket);
    bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    request_message = std::string(buffer, bytes_received); 
    Request req(request_message, server.client_max_body_size);

    // if (req.StatusCode != 200)
    // {
    //     this->data = Return_Error_For_Bad_Request(req.StatusCode);
    //     int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
    //     close(client_socket);
    //     if (num_sent == -1) 
    //     {
    //         std::cout << "Error sending data to client";
    //         close(client_socket);
    //         return;
    //     }
    // }
    

    this->error_pages = server.error_pages;
    this->cookies = parse_cookies(request_message);
    this->cookies_part = manage_cookies_session_server();
    // if (!server.server_name.empty())
    // {
    //     size_t num_pos = req.Host.find(":");
    //     if (req.Host.substr(0, num_pos) != server.server_name)
    //     {
    //         this->data = "HTTP/1.1 503 Service Unavailable\r\nContent-type: text/html\r\n" + this->cookies_part + "\r\n";
    //         this->data += Return_File_Content(server.error_pages["503"]);
    //         int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
    //         close(client_socket);
    //         if (num_sent == -1) 
    //         {
    //             std::cout << "Error sending data to client";
    //             close(client_socket);
    //             return;
    //         }
    //     }
    // }


    if (req.StatusCode != 200)
        this->data = Return_Error_For_Bad_Request(req.StatusCode);

    else 
    {
        if (req.is_Cgi)
        {
            std::string str = req.Path;
            std::string filename;
            std::string lang;
            std::string root_plus_file;
            std::size_t found = str.find_last_of("/");
            if (found != std::string::npos) {
                filename = str.substr(found + 1);
                str = str.substr(0, found);
            }

            if (str.empty())
            {
                size_t lang_pos = filename.find(".");
                lang = filename.substr(lang_pos + 1);
                if (lang == "pl" || lang == "cgi")
                    lang = ".cgi";
                root_plus_file = server.root + req.Path.substr(1);
                std::ifstream file(root_plus_file.substr(1), std::ios::binary);
                if (file)
                    this->data = Cgi_Handler(req, root_plus_file, NULL, lang, server, this->cookies_part);
                else
                {
                    this->data = Cgi_Handler(req, root_plus_file, NULL, lang, server, this->cookies_part);
                    if (req.cgiStatus == 404)
                        this->data += Return_File_Content(server.error_pages["404"]);
                    else if (req.cgiStatus == 403)
                        this->data += Return_File_Content(server.error_pages["403"]);
                    else if (req.cgiStatus == 500)
                        this->data += Return_File_Content(server.error_pages["500"]);
                }
                int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
                close(client_socket);
                if (num_sent == -1) 
                {
                    std::cout << "Error sending data to client";
                    close(client_socket);
                    return;
                }
                return ;
            }
           
            else if (check_if_url_is_location(str.substr(1), server.Locations))
            { 
                if (check_if_location_has_redirect(str.substr(1), server.Locations))
                {
                    int code = get_redirect_code_for_location(str.substr(1), server.Locations);
                    std::string msg = return_redirect_msg(code);
                    
                    this->data = "HTTP/1.1 " + std::to_string(code) + " " + msg + "\r\nLocation: " + get_redirect_url_for_location(str.substr(1), server.Locations) + "\r\n\r\n";
                    int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
                    if (num_sent == -1) 
                    {
                        std::cout << "Error sending data to client";
                        close(client_socket);
                        return;
                    }
                    close(client_socket);
                    return ;
                }

                std::string root_path = get_root_location(str.substr(1), server.Locations);
                std::string all_path = root_path + filename;
                if (req.Method == "POST")
                {
                        if (Check_upload_Location_Status(str.substr(1), server.Locations))
                        {
                            int check;
                            std::string dir_path = get_root_location(str.substr(1), server.Locations) + Get_upload_Location_Path(str.substr(1),server.Locations);
                            check = parse_upload_post_data(request_message, req.Body, dir_path, client_socket, req.Content_Lenght, bytes_received, buffer);
                            if (check)
                            {
                                this->data = "HTTP/1.1 201 Created\r\nContent-type: text/html\r\n" + cookies_part + "\r\n";
                                this->data += Return_File_Content("/Error_Pages/201.html");
                            }
                            else if (check == -1)
                            {
                                this->data = "HTTP/1.1 500 Internal Server Error\r\nContent-type: text/html\r\n" + cookies_part + "\r\n";
                                this->data += Return_File_Content("/Error_Pages/500.html");
                            }
                            else
                            {
                                this->data = Cgi_Handler(req, all_path, NULL, get_location(str.substr(1), server.Locations).CgiLang, server, this->cookies_part);
                            }
                        }
                }

                else 
                {
                    std::ifstream file(all_path.substr(1), std::ios::binary);
                    if (file)
                        this->data = Cgi_Handler(req, all_path, NULL, get_location(str.substr(1), server.Locations).CgiLang, server, this->cookies_part);
                    else
                    {
                        this->data = Cgi_Handler(req, all_path, NULL, get_location(str.substr(1), server.Locations).CgiLang, server, this->cookies_part);
                        if (req.cgiStatus == 404)
                            this->data += Return_File_Content(server.error_pages["404"]);
                        else if (req.cgiStatus == 403)
                            this->data += Return_File_Content(server.error_pages["403"]);
                        else if (req.cgiStatus == 500)
                            this->data += Return_File_Content(server.error_pages["500"]);
                    }
                }


                int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
                if (num_sent == -1) 
                {
                    std::cout << "Error sending data to client";
                    close(client_socket);
                    return;
                }
                close(client_socket);
                return ;
            }
        }

        try {

            full_path = root_path + req.Path.substr(1);
        }
        catch(const std::exception &e)
        {
            std::cout << "error => " << e.what() << std::endl;
        }

        if (check_if_url_is_location(req.Path.substr(1), server.Locations))
        {
            if (check_if_location_has_redirect(req.Path.substr(1), server.Locations))
            {
                int code = get_redirect_code_for_location(req.Path.substr(1), server.Locations);
                std::string msg = return_redirect_msg(code);
                    
                this->data = "HTTP/1.1 " + std::to_string(code) + " " + msg + "\r\nLocation: " + get_redirect_url_for_location(req.Path.substr(1), server.Locations) + "\r\n\r\n";
                int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
                if (num_sent == -1) 
                {
                    std::cout << "Error sending data to client";
                    close(client_socket);
                    return;
                }
                close(client_socket);
                return ;
            }
            
            full_path = get_root_location(req.Path.substr(1), server.Locations);
            tmp_path = full_path;
            tmp_index = get_index_location(req.Path.substr(1), server.Locations);
            tmp_methods = get_allowed_methods(req.Path.substr(1), server.Locations);
            path_check = tmp;
        }
        else if (tmp == path_check && req.Path != "/")
        {
            full_path = tmp_path + req.Path;
        }
            if (tmp == path_check && req.Path != server.root 
            && Check_is_method_allowed(req.Method, tmp_methods))
            {
                if (Check_Cgi_Location_Status(req.Path.substr(1), server.Locations))
                {
                   if (check_if_location_has_redirect(req.Path.substr(1), server.Locations))
                    {
                        int code = get_redirect_code_for_location(req.Path.substr(1), server.Locations);
                        std::string msg = return_redirect_msg(code);
                    
                        this->data = "HTTP/1.1 " + std::to_string(code) + " " + msg + "\r\nLocation: " + get_redirect_url_for_location(req.Path.substr(1), server.Locations) + "\r\n\r\n";
                        int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
                        if (num_sent == -1) 
                        {
                            std::cout << "Error sending data to client";
                            close(client_socket);
                            return;
                        }
                        close(client_socket);
                        return ;
                    }
                    
                    std::string root_path = get_root_location(req.Path.substr(1), server.Locations);
                    std::string all_path = "/" + serve_index_for_cgi(root_path, get_index_location(req.Path.substr(1), server.Locations));

                    if (req.Method == "POST")
                    {
                        if (Check_upload_Location_Status(req.Path.substr(1), server.Locations))
                        {
                            int check;
                            std::string dir_path = get_root_location(req.Path.substr(1), server.Locations) + Get_upload_Location_Path(req.Path.substr(1),server.Locations);
                            check = parse_upload_post_data(request_message, req.Body, dir_path, client_socket, req.Content_Lenght, bytes_received, buffer);
                            if (check)
                            {
                                this->data = "HTTP/1.1 201 Created\r\nContent-type: text/html\r\n" + cookies_part + "\r\n";
                                this->data += Return_File_Content("/Error_Pages/201.html");
                            }
                            else if (check == -1)
                            {
                                this->data = "HTTP/1.1 500 Internal Server Error\r\nContent-type: text/html\r\n" + cookies_part + "\r\n";
                                this->data += Return_File_Content("/Error_Pages/500.html");
                            }
                            else
                            {
        
                                this->data = Cgi_Handler(req, all_path, NULL, get_location(req.Path.substr(1), server.Locations).CgiLang, server, this->cookies_part);
                            }
                        }
                    }
                    else
                    {
                        std::ifstream file(all_path.substr(1), std::ios::binary);
                    
                        if (file)
                        {
                            this->data = Cgi_Handler(req, all_path, NULL, get_location(req.Path.substr(1), server.Locations).CgiLang, server, this->cookies_part);
                        }
                        else
                        {
                            this->data = Cgi_Handler(req, all_path, NULL, get_location(req.Path.substr(1), server.Locations).CgiLang, server, this->cookies_part);
                            if (req.cgiStatus == 404)
                                this->data += Return_File_Content(server.error_pages["404"]);
                            else if (req.cgiStatus == 403)
                                this->data += Return_File_Content(server.error_pages["403"]);
                            else if (req.cgiStatus == 500)
                                this->data += Return_File_Content(server.error_pages["500"]);
                        }
                    }
                    }

                else
                {
                    if (req.Method == "POST")
                    {
                        if (Check_upload_Location_Status(req.Path.substr(1), server.Locations))
                        {
                            int check;
                            std::string dir_path = get_root_location(req.Path.substr(1), server.Locations) + Get_upload_Location_Path(req.Path.substr(1),server.Locations);
                            check = parse_upload_post_data(request_message, req.Body, dir_path, client_socket, req.Content_Lenght, bytes_received, buffer);
                            if (check)
                            {
                                this->data = "HTTP/1.1 201 Created\r\nContent-type: text/html\r\n" + cookies_part + "\r\n";
                                this->data += Return_File_Content("/Error_Pages/201.html");
                            }
                            else if (check == -1)
                            {
                                this->data = "HTTP/1.1 500 Internal Server Error\r\nContent-type: text/html\r\n" + cookies_part + "\r\n";
                                this->data += Return_File_Content("/Error_Pages/500.html");
                            }
                            else
                            {
                                Response res(full_path, "GET", req.Content_Type,
                                client_socket, req.is_Cgi, tmp_index, get_location(req.Path.substr(1), server.Locations).autoindex, full_path, req.Path, true, cookies_part, server.error_pages);
                                this->data = res.res_to_client;
                            }
                        }
                        else
                        {
                            Response res(full_path, "GET", req.Content_Type,
                            client_socket, req.is_Cgi, tmp_index, get_location(req.Path.substr(1), server.Locations).autoindex, full_path, req.Path, true, cookies_part, server.error_pages);
                            this->data = res.res_to_client;
                        }
                    }
                    else 
                    {
                        Response res(full_path, req.Method, req.Content_Type,
                        client_socket, req.is_Cgi, tmp_index, server.autoindex, full_path, req.Path, true, cookies_part, server.error_pages);
                        this->data = res.res_to_client;
                    }
                }


            }
            else if (req.Path == "/" && !Check_is_method_allowed(req.Method, server.allowed_method))
            {
                this->data = "HTTP/1.1 405 Method Not Allowed\r\nContent-type: text/html\r\n" + cookies_part + "\r\n";
                this->data += Return_File_Content(server.error_pages["405"]);
            }
            else if (Check_is_method_allowed(req.Method, server.allowed_method) && !check_if_url_is_location(req.Path.substr(1), server.Locations))
            {
                Response res(full_path, req.Method, req.Content_Type,
                client_socket, req.is_Cgi, server.index, server.autoindex, full_path, req.Path, false, cookies_part, server.error_pages);
                this->data = res.res_to_client;
            }
            else if (Check_is_method_allowed(req.Method, get_location(req.Path.substr(1), server.Locations).allowed_method) && check_if_url_is_location(req.Path.substr(1), server.Locations))
            {
                Response res(full_path, req.Method, req.Content_Type,
                client_socket, req.is_Cgi, tmp_index, get_location(req.Path.substr(1), server.Locations).autoindex, full_path, req.Path, true, cookies_part, server.error_pages);
                this->data = res.res_to_client;
            }
            else
            {
                this->data = "HTTP/1.1 405 Method Not Allowed\r\nContent-type: text/html\r\n" + cookies_part + "\r\n";
                this->data += Return_File_Content(server.error_pages["405"]);
            }
            
    }
    
    int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
    close(client_socket);
    if (num_sent == -1) 
    {
        std::cout << "Error sending data to client";
        close(client_socket);
        return;
    }
}

