#include "Server.hpp"

Server::Server(Config config)
{
    path_check = 0;
    connection(config.Servers);
}


Client::Client()
{

}


void Server::connection(std::vector<ServerBlock> &servers)
{
    this->sockets = setup_sockets(servers);
    std::vector<pollfd> pollfds = create_pollfds(servers);
    std::vector<Client> clients;

    int tmp = 0;
    while(1)
    {
        int ready_count = poll(&pollfds[0], pollfds.size(), -1);
        if (ready_count == -1) {
            std::cout << "Error in poll" << std::endl;
            exit(1);
        }

        for (int i = 0; i < pollfds.size(); i++) {
            if (pollfds[i].revents & POLLIN) {
                if (pollfds[i].fd == servers[i].sock_fd) 
                {
                   
                    handle_new_connection(servers[i].sock_fd, pollfds);
                    tmp = i;
                }
                 else 
                 {
                    respond_to_clients(pollfds[i].fd, root_paths[tmp], servers[tmp], tmp);
                 }
            }
        }
    }
}

std::vector<int> Server::setup_sockets(std::vector<ServerBlock> &servers)
{
    std::vector<int> ret_sockets;

    for (int i =0; i < servers.size(); i++)
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


std::string get_index_file_name_cgi(std::vector<std::string> index, std::string path)
{
    for (std::vector<std::string>::iterator it = index.begin(); it != index.end(); ++it) 
    {
         std::ifstream file(path.substr(1) + *it, std::ios::binary);
         if (file)
            return (path.substr(1) + *it);
    }
    return ("no");
}



std::string serve_index_for_cgi(std::string Path, std::vector<std::string> index_files)
{
    std::string correct_index;
    correct_index = get_index_file_name_cgi(index_files, Path);
    return (correct_index);
}

void Server::respond_to_clients(int client_socket, std::string root_path, ServerBlock server, int tmp)
{
    char buffer[1024];
    std::string full_path;
    int num_bytes = recv(client_socket, buffer, sizeof(buffer), 0);
    if (num_bytes == -1) 
    {
        std::cout<< "socket => " << client_socket << std::endl;
        perror("Error receiving data from client");
        close(client_socket);
        return;
    } 
    else if (num_bytes == 0) 
    {
        close(client_socket);
        return;
    }
    Request req(buffer);



    if (req.StatusCode != 200)
        this->data = Return_Error_For_Bad_Request(req.StatusCode);

    else 
    {
        if (req.is_Cgi)
        {
            std::string str = req.Path;
            std::string sub = "/index.php";

            std::size_t found = str.find(sub);
            if (found != std::string::npos) {
                str.erase(found, sub.length());
            }
            if (check_if_url_is_location(str.substr(1), server.Locations))
            { 
                if (check_if_location_has_redirect(str.substr(1), server.Locations))
                {
                    this->data = "HTTP/1.1 301 Moved Permanently\r\nLocation: " + get_redirect_url_for_location(str.substr(1), server.Locations) + "\r\n\r\n";
                    int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
                    close(client_socket);
                    return ;
                }

                std::string root_path = get_root_location(str.substr(1), server.Locations);
                std::string all_path = "/" + serve_index_for_cgi(root_path, get_index_location(str.substr(1), server.Locations));
                this->data = Cgi_Handler(all_path, NULL);
                int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
                close(client_socket);
                return ;
            }
        }
        full_path = root_path + req.Path.substr(1);
        if (check_if_url_is_location(req.Path.substr(1), server.Locations))
        { 
            if (check_if_location_has_redirect(req.Path.substr(1), server.Locations))
            {
                this->data = "HTTP/1.1 301 Moved Permanently\r\nLocation: " + get_redirect_url_for_location(req.Path.substr(1), server.Locations) + "\r\n\r\n";
                int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
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

        // cookie_handler(buffer);
        
        
            if (tmp == path_check && req.Path != server.root 
            && Check_is_method_allowed(req.Method, tmp_methods))
            {
                if (Check_Cgi_Location_Status(req.Path.substr(1), server.Locations))
                {   
                   if (check_if_location_has_redirect(req.Path.substr(1), server.Locations))
                    {
                        this->data = "HTTP/1.1 301 Moved Permanently\r\nLocation: " + get_redirect_url_for_location(req.Path.substr(1), server.Locations) + "\r\n\r\n";
                        int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
                        close(client_socket);
                        return ;
                    }
                    std::string root_path = get_root_location(req.Path.substr(1), server.Locations);
                    std::string all_path = "/" + serve_index_for_cgi(root_path, get_index_location(req.Path.substr(1), server.Locations));
                    this->data = Cgi_Handler(all_path, NULL);
                }
                else
                {
                    Response res(full_path, req.Method, req.Content_Type,
                    client_socket, req.is_Cgi, tmp_index, server.autoindex, full_path, req.Path, true);
                    this->data = res.res_to_client;
                }


            }
            else if (Check_is_method_allowed(req.Method, server.allowed_method))
            {
                //std::cout << req.Method << std::endl;
                if (req.Method == "POST")
                    parse_upload_post_data(buffer); 

                Response res(full_path, req.Method, req.Content_Type,
                client_socket, req.is_Cgi, server.index, server.autoindex, full_path, req.Path, false);
                this->data = res.res_to_client;
            }
            else
            {
                Response res(full_path, "No", req.Content_Type,
                client_socket, req.is_Cgi, server.index, server.autoindex, full_path, req.Path, false);
                this->data = res.res_to_client;
            }
    }

    
    int num_sent = send(client_socket, this->data.c_str(), this->data.size(), 0);
    close(client_socket);
    if (num_sent == -1) 
    {
        perror("Error sending data to client");
        close(client_socket);
        return;
    }
}

