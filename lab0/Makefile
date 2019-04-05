.SILENT:

default:
	gcc -g -Wall -Wextra lab0.c -o lab0

check: default

	#testing input to output copying:
	touch textin.txt; \
	echo abc > textin.txt; \
	./lab0 --input=textin.txt --output=textout.txt ; \
	if [ $$? -eq 0 ]; \
	then \
		echo "SUCCESS: Copied text with correct exit code" ; \
	else \
		echo "FAILED IO: Copied text with incorrect exit code" ; \
	fi ; \

	if cmp -s textin.txt textout.txt ; \
	then \
		echo "SUCCESS: Copied text from input to output successfully!" ; \
	else \
		echo "FAILED IO: Copying text from input to output failed" ; \
	fi ; \

#test incorrect arguments:
	./lab0 --inupt=abc ; \
	if [ $$? -eq 1 ]; \
	then \
		echo "SUCESS: argument error detected" ; \
	else \
		echo "FAILED ARGERR: argument error detected incorrectly" ; \
	fi ; \

#test input error:
	./lab0 --input=abc; \
	if [ $$? -eq 2 ]; \
	then \
		echo "SUCCESS: input error detected" ; \
	else \
		echo "FAILED INPTERR: input error detected incorrectly" ; \
	fi ; \

#test output error:
	chmod -w textout.txt
	./lab0 --input=textin.txt --output=textout.txt ; \
	if [ $$? -eq 3 ]; \
	then \
		echo "SUCCESS: output error detected" ; \
	else \
		echo "FAILED OUTERR: output error detected incorrectly" ; \
	fi ; \

#test segfault catching:
	./lab0 --catch  --segfault; \
	if [ $$? -eq 4 ]; \
	then \
		echo "SUCCESS: segfault detected correctly" ; \
	else \
		echo "FAILED SEGERR: segfault detected incorrectly" ; \
	fi ; \

#test dumpcore:
	./lab0 --catch --dump-core --segfault ; \
	if [ $$? -eq 139 ]; \
	then \
		echo "SUCCESS: dumpcore behaves correctly" ; \
	else \
		echo "FAILED DUMPERR: dumpcore behaves incorrectly" ; \
	fi 

clean:
	rm -R -f lab0-505000422.tar.gz *.txt *.o lab0 

dist:
	tar -czvf lab0-505000422.tar.gz lab0.c Makefile backtrace.png breakpoint.png README


