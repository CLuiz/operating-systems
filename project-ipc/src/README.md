Enter any explanations or comments that you would like the TAs to read here.

### Part 1
First was to integrate cURL into the handler for the proxy as well as make it multi-threaded which entailed
setting up the library prior to calling the handler.  Once that was complete it was migrating the handler for file
writing to the handler for cURL writing using their example of in memory write.  The biggest challenge was how to properly
handle the errors from cURL and client (many posts on Piazza had thankfully encountered this issue and layed out their
solutions) and properly writing the correct size file with gfs_send (which took a lot of trial and error as well as
Piazza assistance).

For the first part of the project I relied heavily on the cURL library examples listed below:
 - https://curl.haxx.se/libcurl/c/sendrecv.html
 - https://curl.haxx.se/libcurl/c/multithread.html
 - https://curl.haxx.se/libcurl/c/getinmemory.html

### Part 2
Explain the design of the proxy-cache interactions, including what mechanisms you chose and a step-by-step
walk-through of the proxy-cache interactions.

Design

I tried to be as intuitive with how I understood IPC and shared memory to operate, along with following resources I could
piece together from various internet sources (collection below).  Here is my basic idea:

    1.  The web proxy first initiates all shared memory constructs and therefore manages them.  I setup to map the
    shared memory as well initiate the message queues and semaphores to communicate between the two processes.
    2. The handler then initiates communication with the simplecached process first by taking the response from a
    client and processing that information via message queue to the simplecached process (which is blocked on message receive).
    The handler then waits with a locked semaphore (thanks to TA Jose Alessandro Vasconcelos for pointing out dual message
    queues was not a valid form of synchronization) for the simplecached process to respond.
    3.  The simplecached next processes the message, opened the shared memory and checked the local directory for the file,
    and then responds back, via semaphore unlock, to the web proxy handler after writing some file information to
    the shared memory structure.
    4.  If the file existed, the simplecached process continues until it enters the loop to read in data and waits for the
    handler to unlock signaling its ready.  Meanwhile the handler keeps processing, signaling back to the client with the
    appropriate header.  If the file existed it also enters its data transfer loop and first signals to the simplecached
    process to proceed with reading in data and then locks while the simplecached process does so.
    5.  Finally the webproxy handler and simplecached processes continue to rotate blocking on semaphores while the one was
    busy reading the buffer from the shared memory the other waited on its semaphore until it received notification
    it could proceed to send more data to the client.
    6.  When the file size was completely transferred both process exited their loops and the handler proceeds back to its thread
    queue while the simplecached process loops again waiting for another message in the queue.  Simplecached main process
    initiates multiple threads that obviously follow this process therefore if multiple requests are made by the handler
    they can each recieve their own respective message and attach themselves to that shared memory.
    7.  On any signals both processes also had clean-up methods for being able to effeciently unlink and clean up all
    message queues and shared memory constructs.

Observations

My biggest observation was how difficult it was to share the buffers between the two processes.  Linking up the processes actually
proved fairly trivial and therefore passing data via message queue was very straightforward.  Shared memory was actually not
incredibly difficult when it came to small data like the size of the file or something similar but, the actual contents proved
very challenging.  I think focusing on making text files of various sizes assisted a great deal.

Thanks again to the TA that pointed out how my message queue blocking system was inadequate (even though it worked locally, it
obviously was not a viable solution due to the points he made about all processes seeing that same message queue).  When I
introduced semaphores in the same shared memory space for synchronization that problem was addressed however I found a new issue,
race conditions.  That took a great deal of debugging and shuffling to figure out where the root cause of the race was taking place.

Overall the project was very educational and taught a great deal about how shared memory constructs work and IPC mechanisms operate.
There were many times I wanted to hand in the project as is due to the frustrations I continually encountered, especially with the
shared memory buffers and the semaphores.

References:
 - http://www.ibm.com/developerworks/aix/library/au-spunix_sharedmemory/
 - http://cboard.cprogramming.com/c-programming/165199-initialize-1d-2d-array-shared-memory-posix.html
 - https://youtu.be/i0XUbhIBbEc
 - http://stackoverflow.com/questions/3056307/how-do-i-use-mqueue-in-a-c-program-on-a-linux-based-system
 - https://www.cs.cf.ac.uk/Dave/C/node25.html
 - http://stackoverflow.com/questions/8359322/how-to-share-semaphores-between-processes-using-shared-memory
