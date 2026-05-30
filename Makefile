CC = gcc
CFLAGS = -Wall -Wextra -std=c11

all: minls

minls: minls.c minfs.c minfs.h
$(CC) $(CFLAGS) -o minls minls.c minfs.c

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

clean:
rm -f minls *.o

.PHONY: all test clean
