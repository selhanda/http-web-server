#!/usr/bin/perl

# Modules used
use strict;
use warnings;

# print("Content-Type: text/html;\r\n\r\n");
use CGI;
my $cgi = CGI->new();
print $cgi->header;

# Print function
print("<h1>Hello World from perl location cgi!</h1>\n");