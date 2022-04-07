ONVIF / GSOAP by example
========================

In this example, I demonstrate how to retrieve a snapshot URI from an ONVIF complaint IP camera and download it locally.
You can view the complete blog post associated with this project on [my website](http://shoaib-ahmed.com/onvif-gsoap-in-c-by-example/).

The code is well commented and should be easy to read.


Prerequisites
-------------
You will need proper version ssl(1.0.x) and [curl](https://curl.haxx.se/) to build the app.
```
for debian
sudo apt install libssl1.0-dev
sudo apt install libcurl4-gnutls-dev
```


Usage
-----
Clone the example code from github:

```
git clone https://github.com/Sufi-Al-Hussaini/onvif-gsoap-by-example
```

Now, all you have to do is:
```
cd onvif-by-example
make
```

Run the program:
```
./ipconvif -cIp '<camera-ip>' -cUsr '<camera-username>' -cPwd '<camera-password>'
  ex) ./ipconvif -cIp '192.168.0.5' -cUsr 'admin' -cPwd 'adminpassword'
```


Credits
-------
The base code for this project was adapted from the [gsoap-onvif github repo](https://github.com/tonyhu/gsoap-onvif) originally written by [tonyhu](https://github.com/tonyhu). 
