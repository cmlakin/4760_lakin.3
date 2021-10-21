Corrina Lakin
10/20/2021
Project 3: 	Switch out Bakery Algorithm from Project 2 with message queue's
github: https://github.com/cmlakin/4760_lakin.3.git

How to run: DO NOT RUN

			If run with: ./runsim 2
				-It is working up to calling the queue manager, but then gets 
				a long pause that I have to use CTRL C to exit
			If run with ./runsim 2 < testing.data
				-There is something wrong with the message's that I haven't figured
				out yet, so you will get an infinite loop of the msgsnd and msgrcv
				with an invalid arguement error. 

How to compile: make
How to clean:	make clean

At first it seemed as though it would be simpler to just reuse the code from
Project 2 and swap out the bakery algorithm with message queues. I did not use 
the initlicense() in Project 2, but did in this project. I got lost a couple of
times from starting a part but not fully finishing it and not leaving a comment 
of exectly where to pick up. I struggled with some of the syntax and signalling
is still a bit confusing, but I understood it better this time around. 

You keep reminding us to work in small parts and get things going and working.
I did not do that the last couple of days. I would meet with my tutor to figure 
out what needed to be done, mark sections to work on and then go back and check.
But the biggest problem that I had was that while I constantly used make to check
for errors while removing old parts and adding new parts, at some point I stopped
running the program to make sure it was working. I guess I just assumed it would 
since I didn't have any errors. Since I didn't make my usual comments of 'working 
to this point' I don't remember at what point I stopped doing that. 

