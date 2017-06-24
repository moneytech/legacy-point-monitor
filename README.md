# legacy-point-monitor

This repo contains two (useless) applications:
* one that stores `(x,y)` point pairs into a shared memory segment.
* another that reads points pairs from the shared memory segment.

```bash
make install
cd bin
./monitor_shm

# in another terminal
cd bin
./install_data ../points.dat
```

(An example of a small, ugly legacy application for use in a blog post)
