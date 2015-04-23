Websocket server
==========================
This repository contains the GSoC 2015 task

Task
--------
Create a script (on arbitrary language) that will receive messages from syslog-ng. The script receive the messages on stdin and send it via websocket to a
page (you need to create a simple page that can receive the websocket messages, design of the page doesn’t matter).
The script writes the logs to the file too.

Compiling and running
--------
You can compile and run the program this way:

    $ gcc main.c -o main -lwebsockets -lpthread
    $ ./main

Of course, one will need all the dependencies (libwebsocket3, libwebsocket-dev) installed too.

Compiling and running
--------
The syslog-ng.conf file should paste to "/etc/syslog-ng/conf.d". After the paste, you need reload the syslog-ng service.

You should change these
--------
 * main.c: You can give the output file in the code. The variable name: "path".
 * page.html: You can give the destination ip and port.
 * syslog-ng.conf: Here you have to give the path of the program which you compiled
