CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Wno-unused

all: minls minget

minls: minls.c minfs.c minfs.h
	$(CC) $(CFLAGS) -o minls minls.c minfs.c
minget: minget.c minfs.c minfs.h
	$(CC) $(CFLAGS) -o minget minget.c minfs.c

test: minls
	@echo ""
	@echo "===== Test 1: Root Directory ====="
	./minls ~pn-cs453/Given/Asgn5/Images/TestImage

	```
	@echo ""
	@echo "===== Test 2: Hello File ====="
	./minls ~pn-cs453/Given/Asgn5/Images/TestImage /Hello

	@echo ""
	@echo "===== Test 3: Verbose ====="
	./minls -v ~pn-cs453/Given/Asgn5/Images/TestImage /Hello

	@echo ""
	@echo "===== Test 4: BigDirectories ====="
	./minls ~pn-cs453/Given/Asgn5/Images/BigDirectories

	@echo ""
	@echo "===== Test 5: SmallBlocks ====="
	./minls ~pn-cs453/Given/Asgn5/Images/SmallBlocks--1k

	@echo ""
	@echo "===== Test 6: Partitioned ====="
	./minls -p 0 ~pn-cs453/Given/Asgn5/Images/Partitioned /
	```

#HandIn Commands
turnin: minls.c minget.c minfs.c minfs.h Makefile README
	handin pn-cs453 asgn5 README Makefile minfs.c minfs.h minls.c minget.c
	handin pn-cs453 asgn5 > submitted

clean:
	rm -f minls minget *.o core* *DETAILS*

.PHONY: all test clean
