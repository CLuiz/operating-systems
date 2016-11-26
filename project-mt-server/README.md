# Project: Multi-Threaded Web Server

Please include:

* A high level description of your strategies for implementing the various components.  Anything that you think worth noting should be fine.
* Any insights. We may consider extra points!
* Known bugs
* Credits if you used others’ (publicly available) code.

### Getfile Client and Server
To be completely honest there was hours of trial and error with this portion of the project.  A lot of the test cases threw curve balls (large file lost connection re-attempt?).  I based my design almost exactly on the echo and transfer protocol projects earlier.  Then trial and error on what needs to be done to satisfy the getfile protocol tests.  I know it's not the most effective solution but my limited networking background hindered my ability to really flush out various scenarios.

### Multi-Threaded Client
I actually found a helpful Gist on Github that I thought did a pretty good job illustrating.  Basically using the given gfclient_download code as much as possible the only necessary.  I realized two main things needed to be done, thread creation to allow for multiple client perform functions to take place and creating a structure to pass the request params to the thread.  Below are the sources I referenced during this process.

- Source: https://gist.github.com/silv3rm00n/5821760
- Source: http://stackoverflow.com/questions/21405204/multithread-server-client-implementation-in-c


### Multi-Threaded Web Server
For the web server multi-threading problem I actually relied heavily on the class given diagram of how to design the web server and use the boss-worker model.  It is pretty explicit about how the handler should operate and processes it is responsible for.  I also found the following tutorial useful as well in describing the actions necessary.

- Source: http://www.mario-konrad.ch/wiki/doku.php?id=programming:multithreading:tutorial-04
