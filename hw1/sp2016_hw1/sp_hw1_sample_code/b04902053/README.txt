########  README   ########
##  By b04902053 鄭淵仁  ##

# How to compile my program?

Just run "make".

# Anything I have done besides the basic functionality?

I just followed the problem description and subtasks in the spec.

That is, I wrote the IO multiplexing by select() function, and file locks by fcntl() function, and added a for loop outside the #ifdefs to go through all the file descriptors.

And also, I added a member "file_fd" inside the struct "request" to record the file descriptor we open for the request to read or write.
