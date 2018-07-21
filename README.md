# mongols
a library for c/c++

## dependency

- pcre-devel
- zlib-devel
- openssl-devel

`sudo yum install pcre-devel zlib-devel openssl-devel `

or

`sudo apt-get install libpcre3-dev zlib1g-dev libssl-dev`

## feature

- tcp sever (epoll)
- tcp multi-threading server (epoll+multi-threading)
- http server (epoll)
- http multi-threading server (epoll+multi-threading)
- websocket server
- uitls

## install 

`make clean && make -j2 && sudo make install`

## usage

`pkg-config --libs --cflags mongols`

## example

```cpp

#include <mongols/tcp_server.hpp>
#include <mongols/tcp_threading_server.hpp>
#include <mongols/http_server.hpp>
#include <mongols/ws_server.hpp>
#include <mongols/util.hpp>
#include <iostream>


//websocket server
int main(int,char**){
	int port=9090;
	const char* host="127.0.0.1";
	mongols::ws_server server(host,port);

	auto f=[](const std::string& input
            , bool& keepalive
            , bool& send_to_other
            , std::pair<size_t, size_t>& g_u_id
            , mongols::tcp_server::filter_handler_function& send_to_other_filter){
			keepalive = KEEPALIVE_CONNECTION;
			send_to_other=true;
			return input;
	};
	//server.run(f);
	server.run();
}


/*
//tcp server or tcp multi-threading server
int main(int,char**)
{
	auto f=[](const std::string& input
                , bool& send_to_other
                , std::pair<size_t, size_t>& g_u_id
                , mongols::tcp_server::filter_handler_function& send_to_other_filter){
					send_to_other=true;
					return std::make_pair(input,KEEPALIVE_CONNECTION);
				};
	int port=9090;
	const char* host="127.0.0.1";
	
	mongols::tcp_threading_server
	//mongols::tcp_server

	server(host,port);
	server.run(f);

}
*/


/*
//http server or multi-threading server
int main(int,char**)
{
	auto f=[](const mongols::request&){
		return true;
	};
	auto g=[](const mongols::request& req,mongols::response& res){
		res.content=std::move("hello,world");
		res.status=200;
	};
	int port=9090;
	const char* host="127.0.0.1";
	mongols::http_server 
	//server(host,port,5000,1024,2)
	server(host,port);
	server.run(f,g);
}
*/

```

## apache benchmark

```

[centos@localhost demo]$ cat /proc/cpuinfo| grep "processor"| wc -l
4
[centos@localhost demo]$ cat /proc/cpuinfo| grep "cpu cores"| uniq
cpu cores	: 2
[centos@localhost demo]$ cat /proc/cpuinfo| grep "physical id"| sort| uniq| wc -l
1


```

### ` ab -c1000 -n100000 http://127.0.0.1:9090/ `

```
Server Software:        mongols/0.9.0
Server Hostname:        127.0.0.1
Server Port:            9090

Document Path:          /
Document Length:        13 bytes

Concurrency Level:      1000
Time taken for tests:   8.180 seconds
Complete requests:      100000
Failed requests:        0
Write errors:           0
Keep-Alive requests:    0
Total transferred:      13300000 bytes
HTML transferred:       1300000 bytes
Requests per second:    12225.00 [#/sec] (mean)
Time per request:       81.800 [ms] (mean)
Time per request:       0.082 [ms] (mean, across all concurrent requests)
Transfer rate:          1587.82 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0   66 263.3      7    3020
Processing:     2   11  11.9      9     831
Waiting:        0    8  11.5      7     829
Total:          6   77 265.8     16    3033

Percentage of the requests served within a certain time (ms)
  50%     16
  66%     20
  75%     22
  80%     23
  90%     29
  95%   1017
  98%   1025
  99%   1030
 100%   3033 (longest request)

```

### ` ab -kc1000 -n100000 -H'Connection: keep-alive'  http://127.0.0.1:9090/ `

```

Server Software:        mongols/0.9.0
Server Hostname:        127.0.0.1
Server Port:            9090

Document Path:          /
Document Length:        13 bytes

Concurrency Level:      1000
Time taken for tests:   1.935 seconds
Complete requests:      100000
Failed requests:        0
Write errors:           0
Keep-Alive requests:    100000
Total transferred:      13800000 bytes
HTML transferred:       1300000 bytes
Requests per second:    51671.74 [#/sec] (mean)
Time per request:       19.353 [ms] (mean)
Time per request:       0.019 [ms] (mean, across all concurrent requests)
Transfer rate:          6963.57 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   3.2      0      48
Processing:     8   19   1.4     18      48
Waiting:        0   19   1.4     18      25
Total:          8   19   3.1     18      64

Percentage of the requests served within a certain time (ms)
  50%     18
  66%     19
  75%     19
  80%     19
  90%     20
  95%     21
  98%     23
  99%     28
 100%     64 (longest request)


```