#!/usr/bin/python

import os, sys, mmap, random
#import cProfile

FILE_NAME="testfile"
ROUNDS=1000
BUFFER_SIZE=512

if len(sys.argv) < 2:
   print(f"Usage: time python {sys.argv[0]} [rounds]")
   print("rounds - a number of fsync calls (default=1000)")
   print("\nReturns e.g.:")
   print("# real  0m6,334s")
   print("Where \"real\" shows latency = 6,334s/1000 rounds = 6,334 ms")
   sys.exit(1)

def test_sync():
   WRITTEN=0
   ROUNDS=int(sys.argv[1])
   print(f"Start testing {ROUNDS} rounds...")

   # Open a file
   fd = os.open(FILE_NAME, os.O_RDWR|os.O_CREAT|os.O_DIRECT)
   os.truncate(fd, BUFFER_SIZE)

   m = mmap.mmap(-1, BUFFER_SIZE)

   for i in range (0, ROUNDS):
      os.lseek(fd,os.SEEK_SET,0)
      m[i % BUFFER_SIZE] = random.randint(0, 255)
      WRITTEN += os.write(fd, m)
      os.fsync(fd)

   print(f"Written {WRITTEN} bytes...")

   # Close memory-mapped file
   m.close()

   # Close opened file
   os.close(fd)
   os.remove(FILE_NAME)

#cProfile.run("test_sync()", sort='cumulative')
test_sync()
