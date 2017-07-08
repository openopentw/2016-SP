/* b04902053 鄭淵仁 */
/* README */

# How to compile my program?

Run "make".

Besides, run "make clean" will delete all the "*.o" files.

# Anything I have done besides the basic functionality?

I just followed the problem description and subtasks in the spec.

That is, I first wrote the player.c that always output "5" to the FIFO.

Then, I wrote the judge.c and big_judge.c just as the spec says.

Specially, I use select to wait 3 second for response from player in the
judge, so the program might not work very fast.
