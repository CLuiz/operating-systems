Enter any explanations or comments that you would like the TAs to read here.

This project was pretty straight forward, biggest issue I ran into was allocating memory approprioately.  Step one was
designated in the instructions, create the XDR code necessary for generating the clnt, main, svc, xdr files.  Once I had
figured the best solution for that I focused on the minifyjpeg.c which involved implementing its header files functions.
Finally was implementing the minify_via_rpc.c in which the server connects to the client, simple enough, and then actually
making the rpc call and compressing the image.  This last step proved the most challenging as I kept encountering the 
memory allocation issues.

References:
 - http://www.cprogramming.com/tutorial/rpc/remote_procedure_call_start.html
 - https://www.ibm.com/support/knowledgecenter/ssw_aix_53/com.ibm.aix.progcomm/doc/progcomc/rpc_lang.htm%23a283x91559
 - http://web.cs.wpi.edu/~rek/DCS/D04/SunRPC.html